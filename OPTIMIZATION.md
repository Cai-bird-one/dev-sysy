# 优化说明

本文档描述当前编译器实现的优化。优化覆盖 tensor dialect 规整、Koopa IR pass pipeline、RISC-V 后端代码生成优化、窥孔优化，以及基于活跃变量分析的线性扫描寄存器分配。

## 1. 优化入口

`-perf` 模式下的主要优化流程如下：

1. 前端 lexer/parser 将 `tensor` 作为 `BType` 写入语法树，Koopa 生成阶段输出 tensor dialect。
2. `IrOptimizer` 将 Koopa IR 解析成 `IrModule`，其中 `IrModule::tensor` 保存 tensor dialect。
3. `IrOptimizer` 反复运行 `buildPerfPipeline()`，最多迭代 8 轮，直到 pass 不再产生变化。
4. RISC-V 后端解析优化后的 Koopa IR。
5. 后端使用 IR 层活跃变量分析结果做线性扫描寄存器分配。
6. 指令发射阶段使用更紧凑的 RISC-V 指令选择。
7. 最终汇编经过 RISC-V 窥孔优化器。

相关入口：

- `src/compiler/ir/opt/ir_optimizer.cpp`
- `src/compiler/ir/opt/pipeline.cpp`
- `src/compiler/riscv/riscv_generator.cpp`

## 2. Tensor Dialect 优化

Tensor dialect 已纳入 `IrModule` 结构，解析结果保存在 `IrModule::tensor`。`TensorDialectOptimizationPass` 调用 `src/compiler/ir/opt/tensor/koopa_tensor_optimize.cpp` 中的规整逻辑，只处理 tensor IR 元信息，不改变普通 scalar Koopa 指令。

当前规则：

- 删除形状不变的 `reshape`。
- 删除形状不变的 `broadcast`。
- 合并连续 `reshape`，直接从原始输入 reshape 到最终形状。
- 合并连续 `broadcast`，直接从原始输入广播到最终形状。
- 删除成对抵消的 `transpose`。
- 将 `matmul(transpose(A), B)`、`matmul(A, transpose(B))`、`matmul(transpose(A), transpose(B))` 规整为 `matmul_ta`、`matmul_tb`、`matmul_ta_tb`，把转置作为固定 tensor 的布局标志保留，避免后续 lowering 物化临时转置矩阵。
- 规整连续 `relu`。
- 将 `matmul + add` 规整为 `matmul_bias`。
- 将 `matmul + relu` 规整为 `matmul_relu`。
- 将 `add + relu` 规整为 `add_relu`。
- 将 `matmul + add + relu` 规整为 `matmul_bias_relu`。
- 上述矩阵乘融合会保留 `_ta` / `_tb` 标志，例如 `transpose(A) * B + bias` 后接 `relu` 会规整为 `matmul_bias_relu_ta`。
- 将显式 `broadcast` bias 下沉到 `matmul_bias` / `matmul_bias_relu` 中，复合 op 直接引用原始 bias。
- 对 tensor op 做形状感知公共子表达式消除；`add` / `add_relu` 会先按交换律规整操作数再比较。
- 删除不再被其他 tensor 声明依赖、且不需要作为最终结果保留的中间 tensor 操作。

Tensor 优化只以显式 tensor dialect 为依据，不从普通数组循环中猜测矩阵语义。类似转置、矩阵乘、融合算子等高层矩阵优化必须由固定形状的 `tensor ...` 声明或 tensor op 触发；普通 SysY 二维数组仍走通用 Koopa/RISC-V lowering。

矩阵乘相关规整参考 GotoBLAS/BLIS 的分块、packing、micro-kernel 分层思想，以及 TBLIS 将转置/重排融合进内部划分和 packing、避免显式转置物化的设计。当前实现先在 tensor dialect 中保留可被 lowering 消费的布局标志，后续专用 tensor kernel 可以直接按 `ta/tb` 选择访存顺序和分块策略。

示例：

```koopa
tensor %a: tensor<2x3>
tensor %b: tensor<3x4>
tensor %bias: tensor<1x4>
tensor %bb = broadcast %bias : tensor<2x4>
tensor %m = matmul %a, %b : tensor<2x4>
tensor %sum = add %m, %bb : tensor<2x4>
tensor %out = relu %sum : tensor<2x4>
```

可以规整为：

```koopa
tensor %a: tensor<2x3>
tensor %b: tensor<3x4>
tensor %bias: tensor<1x4>
tensor %out = matmul_bias_relu %a, %b, %bias : tensor<2x4>
```

## 3. Koopa IR 优化 Pipeline

IR 优化 pipeline 在 `buildPerfPipeline()` 中定义，当前顺序如下：

1. `TensorDialectOptimizationPass`
2. `UnusedFunctionEliminationPass`
3. `ConstantFoldingPass`
4. `AlgebraicSimplifyPass`
5. `GlobalValuePropagationPass`
6. `LocalLoadStoreForwardingPass`
7. `GlobalScalarLoadForwardingPass`
8. `LocalDeadStoreEliminationPass`
9. `CommonSubexpressionEliminationPass`
10. `LoopInvariantCodeMotionPass`
11. `BranchSimplifyPass`
12. `JumpThreadingPass`
13. `UnreachableBlockEliminationPass`
14. `DeadCodeEliminationPass`
15. `UnusedAllocEliminationPass`
16. `BlockCleanupPass`

这些 pass 会反复运行，直到一轮 pipeline 没有变化，或达到 8 轮迭代上限。

### 3.1 未使用函数删除

`UnusedFunctionEliminationPass` 从 `@main` 出发构造函数调用可达集合，删除不可达函数。

它保留可被 `@main` 直接或间接调用的函数，减少后端处理和输出的无用代码。

### 3.2 常量折叠

`ConstantFoldingPass` 在基本块内传播整数常量，并折叠二元运算：

- `add`
- `sub`
- `mul`
- `div`
- `mod`
- `eq`
- `ne`
- `lt`
- `gt`
- `le`
- `ge`

除数为 0 的 `div` 和 `mod` 不会折叠。

### 3.3 代数化简

`AlgebraicSimplifyPass` 处理简单代数恒等式：

- `x + 0 -> x`
- `0 + x -> x`
- `x - 0 -> x`
- `x * 1 -> x`
- `1 * x -> x`
- `x * 0 -> 0`
- `0 * x -> 0`
- `x / 1 -> x`

该 pass 会在基本块内传播化简后的 value。

### 3.4 全局值传播

`GlobalValuePropagationPass` 基于 CFG 做前向数据流分析，在基本块入口计算稳定的 value facts。

只有所有前驱都一致的事实才会在汇合点保留。它可以跨基本块传播常量和简单别名关系。

### 3.5 局部 Load/Store 转发

`LocalLoadStoreForwardingPass` 针对 `alloc i32` 产生的局部标量内存做保守转发。

典型形式：

```koopa
store %x, %p
%y = load %p
```

若中间没有可能破坏内存状态的指令，`%y` 可以直接替换为 `%x`。遇到 label、terminator、call 或非标量指针 store 时，会清空相关状态。

### 3.6 全局标量 Load 转发

`GlobalScalarLoadForwardingPass` 针对 `alloc i32` 产生的局部标量内存做跨基本块的数据流分析。

该 pass 在每个基本块入口维护已知的标量内存事实。若所有前驱都能证明某个标量地址保存同一个 value，则后续 `load` 可以改写成等价的 value copy。实现上会将：

```koopa
%x = load %slot
```

改写为：

```koopa
%x = add %known, 0
```

后续常量折叠、代数化简、DCE 和寄存器分配可以继续处理这个普通 SSA value。遇到 `call` 或非标量指针 `store` 时会清空内存事实，避免跨副作用错误传播。

### 3.7 局部死 Store 删除

`LocalDeadStoreEliminationPass` 逆序扫描函数，删除对局部标量内存的无用 store。

如果某个 `store` 写入后在被下一次写入覆盖前没有对应 `load`，则该 `store` 可以删除。遇到控制流边界和函数调用时会保守认为所有标量内存都可能活跃。

### 3.8 公共子表达式删除

`CommonSubexpressionEliminationPass` 在基本块内识别重复的纯表达式：

- 算术运算
- 比较运算

对可交换运算会规范化操作数顺序，例如 `a + b` 和 `b + a` 使用同一个表达式 key。重复表达式的结果会被替换为已有 value。

### 3.9 循环不变量外提

`LoopInvariantCodeMotionPass` 通过 CFG 识别自然循环，找到循环 preheader，并将循环内满足条件的纯计算外提到循环前。

当前策略偏保守：

- 只外提 side-effect-free 的纯计算。
- 操作数必须来自循环外，或已经被判定为循环不变量。
- 不外提 `load`、`store`、`call` 等可能依赖内存或有副作用的指令。
- 只在能找到合适 preheader 时移动指令。

### 3.10 分支化简

`BranchSimplifyPass` 化简确定分支：

- `br 0, A, B -> jump B`
- `br 非 0 整数, A, B -> jump A`
- `br cond, A, A -> jump A`

### 3.11 跳转穿透

`JumpThreadingPass` 识别只包含跳转的中间标签，把跳转目标改写为最终目标。

如果某个 `jump` 的目标正好是下一条 label，则直接删除这条跳转。

### 3.12 不可达块删除

`UnreachableBlockEliminationPass` 从入口基本块出发遍历 CFG，删除不可达基本块。

### 3.13 死代码删除

`DeadCodeEliminationPass` 复用 IR 层 `analyzeDenseLiveness` 活跃变量分析结果，删除 live-out 中不再需要、且无副作用的赋值指令。

`alloc` 不会在该 pass 中删除，以避免破坏后续内存布局和指针语义。

### 3.14 无用 Alloc 删除

`UnusedAllocEliminationPass` 删除已经没有任何使用点的 `alloc` 指令。

该 pass 通常和 load 转发、死 store 删除、DCE 配合生效：当局部标量的 load/store 被优化掉后，原本为了这个变量保留的栈槽也可以被移除，从而减少栈帧大小。

### 3.15 基本块清理

`BlockCleanupPass` 删除同一个基本块中 terminator 之后、下一条 label 之前的不可执行指令。

## 4. 活跃变量分析

活跃变量分析位于 `src/compiler/ir/opt/analysis/liveness_analysis.cpp`。

当前实现包含两层接口：

- `analyzeDenseLiveness`：使用 bit-vector 表示 live-in/live-out，供 DCE 和后端寄存器分配使用。
- `analyzeLiveness`：把 dense 结果转换成 `std::set` 形式，便于测试或调试。

分析过程：

1. 收集每条指令的 use/def。
2. 基于 CFG 逆序迭代。
3. 对每条指令计算 `live_out` 和 `live_in`。
4. 迭代直到不动点。

## 5. RISC-V 指令选择优化

RISC-V 指令发射阶段位于 `src/compiler/riscv/emit/`。除了直接翻译 Koopa 指令外，当前也做了一些局部指令选择优化：

- `add x, imm12` 使用 `addi`。
- `sub x, imm12` 在负立即数可编码时使用 `addi`。
- `and/or` 搭配 12 位立即数时使用 `andi/ori`。
- `mul x, 0` 直接生成 `li 0`。
- `mul x, 1` 直接复用原操作数。
- `mul x, 2^k` 使用 `slli` 替代常量加载和乘法。
- 和 0 比较时直接使用 `zero` 寄存器、`seqz`、`snez` 或 `slt` 组合，避免额外加载 0。
- 数组地址计算中的 stride 若是 2 的幂，使用 `slli` 替代 `li + mul`。
- 已分配到寄存器的 Koopa value 直接从物理寄存器读取，减少栈访问。
- 指针 value 若已经在寄存器中，`load/store` 直接使用寄存器间接寻址。

RISC-V 后端不会仅凭普通数组访问模式识别矩阵转置或矩阵乘法内核；这类高层优化统一保留给显式 tensor dialect，避免把普通二维数组程序误判为 tensor 计算。

## 6. RISC-V 窥孔优化

RISC-V 窥孔优化位于 `src/compiler/riscv/opt/peephole_optimizer.cpp`，在完整汇编生成后运行。

当前规则：

- 删除 `mv x, x`。
- 删除 `addi x, x, 0`。
- 将 `add/or/xor dst, x, zero` 化简为 `mv dst, x`。
- 将 `add/or/xor dst, zero, x` 化简为 `mv dst, x`。
- 将 `sub dst, x, zero` 化简为 `mv dst, x`。
- 将 `mul dst, x, zero` 或 `mul dst, zero, x` 化简为 `li dst, 0`。
- 将 `addi dst, x, 0` 化简为 `mv dst, x`，后续若成为 no-op 再删除。
- 将 `li t2, 2^k` 后紧跟的 `mul t1, t1, t2` 化简为 `slli t1, t1, k`。
- 删除跳转到紧邻下一条 label 的 `j label`。
- 对栈槽 `lw` 做简单值复用；若同一栈地址刚加载到某寄存器，再次加载可改为 `mv` 或直接删除。

遇到 label、汇编伪指令、函数调用、返回和跳转等边界时，窥孔优化会清空状态，保证跨控制流边界的保守正确性。

## 7. 寄存器分配优化

寄存器分配位于 `src/compiler/riscv/regalloc/`。当前实现不使用图着色，而是复用 IR 层活跃变量分析结果，并通过线性扫描完成分配。

主要文件：

- `register_allocator.cpp`：总入口，组织分配流程。
- `register_allocator_analysis.cpp`：从 IR 活跃变量分析结果构造 live interval。
- `register_allocator_linear_scan.cpp`：执行线性扫描分配。
- `register_allocator_call_saves.cpp`：记录跨 call 需要保存的值。
- `register_allocation.*`：保存 value 到物理寄存器的映射和 call-save 信息。

流程：

1. 收集可分配 value，排除 `alloc` 结果。
2. 调用 `analyzeDenseLiveness` 获取每条指令的 live-in/live-out。
3. 根据 use/def/live-in/live-out 构造 live interval。
4. 按 interval 起点排序，执行线性扫描。
5. 对跨 call 活跃的 value 优先使用 callee-saved register。
6. 对函数参数、call 参数和普通临时值使用不同寄存器偏好顺序。
7. 当寄存器不足时，按下一次使用位置和 interval 结束位置选择当前值或已有 active interval 溢出。
8. 指令发射阶段只对必要的 caller-saved value 生成 call 前保存和 call 后恢复。

这种实现复杂度低于图着色，同时能覆盖大量局部变量、长 live range、函数参数和跨调用活跃值。

## 8. 测试与验证

相关测试覆盖：

- IR pass：`src/tests/compiler/ir/opt/ir_optimizer.cpp`
- Tensor dialect：`src/tests/compiler/ir/koopa_tensor_ir.cpp`
- RISC-V 后端：`src/tests/compiler/riscv/riscv_generator.cpp`
- 寄存器分配：`src/tests/compiler/riscv/regalloc/register_allocator.cpp`
- 栈帧和指令发射：`src/tests/compiler/riscv/frame/`、`src/tests/compiler/riscv/emit/`

当前已在 Docker 环境中通过：

- 单元测试：`156/156`
- 性能测试：`467/467`

## 9. 性能迭代结果

完整正确性测试命令：

```sh
autotest -perf -t sysy-testsuit-collection -s lvX /root/compiler
```

历史记录里的 `real time` 是完整 autotest 墙钟时间，包含编译器构建、每个用例编译、RISC-V 汇编链接、qemu 运行和测试调度开销，只用于观察整体测试耗时，不作为最终性能指标：

| 版本或阶段 | 结果 | real time | 说明 |
| --- | ---: | ---: | --- |
| `3d835cdabc5276cb92321d2c5e6358fbf1599733` | `455/467` | `1123.08s` | 旧基线，包含 compile error、TLE 和 WA |
| 当前优化前基线 | `467/467` | `295.86s` | 已完成线性扫描寄存器分配和 LICM 后的基线 |
| 加入跨块标量 load 转发与无用 alloc 删除后 | `467/467` | `247.49s` | 明显减少局部标量栈槽和 load/store |
| 加入乘 2 的幂强度削弱后 | `467/467` | `243.69s` | RISC-V 发射阶段直接用 `slli` |
| 加入零比较专用发射后 | `467/467` | `239.67s` | 避免额外加载 0，直接使用 `zero` |

相对当前优化前基线，历史最好记录耗时减少 `56.19s`，约为原来的 `81.0%`，即约 `1.23x` 加速。相对 `3d835cd` 旧基线，最终从 `455/467` 提升到 `467/467`，整套测试 wall time 约为旧基线的 `21.3%`。

本轮回退验证改用 `scripts/measure_riscv_runtime.py` 统计 qemu 阶段耗时，只累加编译出的 RISC-V 程序运行时间，不计入编译器构建、用例编译、汇编链接和测试框架调度时间：

```sh
python3 scripts/measure_riscv_runtime.py /root/compiler \
  -t sysy-testsuit-collection -s lvX
```

| 版本或阶段 | 结果 | RISC-V 运行时间 | 说明 |
| --- | ---: | ---: | --- |
| `1eaf1d8` 历史最好后端 | `467/467` | `88.954647s` | 导出 `HEAD` 后用同一测试集、同一脚本测得 |
| 当前回退后工作区 | `467/467` | `89.664780s` | 与 `HEAD` 差 `0.710133s`，约 `0.8%` |

慢用例 `matrix-1`、`floyd`、`transpose`、`powmod`、`conv1d`、`mv`、`mv2` 的当前工作区汇编与 `HEAD` 完全一致，因此这 `0.8%` 属于 qemu 单次运行波动，不是代码生成回归。

已回退的负收益或不稳定后端实验包括：

- `mul x, 2^k +/- 1` 展开为移位加减。
- 12 位立即数比较特化。
- 常量条件分支直接发射。
- `getelemptr/getptr` 常量下标发射期折叠。
- RISC-V 窥孔阶段比较分支折叠。
- 普通数组矩阵转置模式识别；矩阵类高层优化应统一挂在固定形状 tensor dialect 上，而不是从通用数组循环推断。
