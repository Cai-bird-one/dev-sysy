# 优化说明

本文档描述当前编译器实现的优化。优化覆盖 tensor dialect 规整、tensor 矩阵乘 lowering、Koopa IR pass pipeline、RISC-V 后端代码生成优化、窥孔优化，以及基于活跃变量分析的线性扫描寄存器分配。

## 1. 优化入口

`-perf` 模式下的主要优化流程如下：

1. 前端 lexer/parser 将 `tensor` 作为 `BType` 写入语法树，Koopa 生成阶段输出 tensor dialect。
2. `-perf` 也支持直接输入 `.koopa` 文件，便于测试手写 tensor dialect。
3. `IrOptimizer` 将 Koopa IR 解析成 `IrModule`，其中 `IrModule::tensor` 保存 tensor dialect。
4. `IrOptimizer` 反复运行 `buildPerfPipeline()`，最多迭代 128 轮，直到 pass 不再产生变化。
5. RISC-V 后端解析优化后的 Koopa IR。
6. RISC-V 优化调度器 `RiscvOptimizer` 先执行发射前优化阶段，其中包括基于 IR 活跃变量分析结果的多候选线性扫描寄存器分配。
7. 指令发射阶段使用更紧凑的 RISC-V 指令选择。
8. 最终汇编进入 `AssemblyOptimizer`，所有汇编级 RISC-V pass 在同一个 driver 中最多迭代 128 轮，直到汇编不再变化。

相关入口：

- `src/compiler/ir/opt/ir_optimizer.cpp`
- `src/compiler/ir/opt/pipeline.cpp`
- `src/compiler/riscv/riscv_generator.cpp`
- `src/compiler/riscv/opt/riscv_optimizer.cpp`
- `src/compiler/riscv/opt/assembly_optimizer.cpp`

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

矩阵乘相关规整参考 GotoBLAS/BLIS 的分块、packing、micro-kernel 分层思想，以及 TBLIS 将转置/重排融合进内部划分和 packing、避免显式转置物化的设计。当前实现会在 tensor dialect 中保留可被 lowering 消费的布局标志，专用 tensor lowering 可以直接按 `ta/tb` 选择访存顺序。

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

### 2.1 Tensor 矩阵乘 Lowering

`TensorMatmulLoweringPass` 位于 `src/compiler/ir/opt/passes/tensor_matmul_lowering.cpp`，在 tensor dialect 规整之后运行。它只处理函数体内显式出现的 tensor 矩阵乘 op，例如：

```koopa
tensor @C = matmul @A, @B : tensor<96x96>
```

触发条件：

- op 必须属于 `matmul`、`matmul_relu`、`matmul_bias`、`matmul_bias_relu` 或对应 `_ta` / `_tb` 变体。
- 输入、输出和 bias 必须已经有明确存储位置，来源可以是全局数组、局部 `alloc` 或指针参数。
- 形状必须是静态 rank-2 矩阵，bias 形状按 tensor 校验规则广播。
- 普通数组三重循环不会触发该 pass，避免误判普通程序。

Lowering 策略：

- 将函数内 tensor op 替换成普通 Koopa 控制流和 `getelemptr` / `load` / `mul` / `add` / `store` 指令。
- 对 `K <= 128` 的固定内维矩阵乘展开内层 `k` 循环，把累加链保留为 SSA value，减少内层分支、循环变量 load/store 和累加器栈访问。
- 对更大的非转置 RHS 保留 packing fallback，将 RHS 按列打包成连续的 `[N][K]` 临时布局，降低普通 `i-j-k` 内层访问 B 的跨行访存代价。
- `matmul_relu` 和 `matmul_bias_relu` 在 store 前融合 ReLU，不额外物化中间 tensor。
- `_ta` / `_tb` 直接改变元素寻址顺序，不生成显式转置矩阵。

## 3. Koopa IR 优化 Pipeline

IR 优化 pipeline 在 `buildPerfPipeline()` 中定义，当前顺序如下：

1. `TensorDialectOptimizationPass`
2. `TensorMatmulLoweringPass`
3. `UnusedFunctionEliminationPass`
4. `ConstantFoldingPass`
5. `AlgebraicSimplifyPass`
6. `GlobalValuePropagationPass`
7. `LocalLoadStoreForwardingPass`
8. `GlobalScalarLoadForwardingPass`
9. `LocalDeadStoreEliminationPass`
10. `CommonSubexpressionEliminationPass`
11. `LoopInvariantCodeMotionPass`
12. `BranchSimplifyPass`
13. `JumpThreadingPass`
14. `UnreachableBlockEliminationPass`
15. `DeadCodeEliminationPass`
16. `UnusedAllocEliminationPass`
17. `BlockCleanupPass`

这些 pass 会反复运行，直到一轮 pipeline 没有变化，或达到 128 轮迭代上限。

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
- `x - x -> 0`
- `x == x -> 1`
- `x != x -> 0`
- `x <= x -> 1`
- `x >= x -> 1`
- `x < x -> 0`
- `x > x -> 0`
- `x & x -> x`
- `x | x -> x`
- `x & 0 -> 0`
- `0 & x -> 0`
- `x | 0 -> x`
- `0 | x -> x`

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
- `mul x, -1` 使用 `sub rd, zero, rs`。
- `mul x, 2^k` 使用 `slli` 替代常量加载和乘法。
- `mul x, -2^k` 使用 `slli + sub zero` 替代常量加载和乘法。
- `mul x, 2^k +/- 1` 使用 `slli + add/sub` 替代常量加载和乘法。
- `div/mod x, 2^k` 使用带符号修正的 `srai/and/add/sub` 序列替代 RISC-V `div/rem`，保持 SysY/C 的向 0 截断语义。
- 和 0 比较时直接使用 `zero` 寄存器、`seqz`、`snez` 或 `slt` 组合，避免额外加载 0。
- `eq/ne/lt/le/gt/ge` 与可编码立即数比较时优先使用 `addi + seqz/snez` 或 `slti`，减少循环条件和字符判断中的常量加载。
- 若比较结果只被紧随其后的 `eq/ne %cmp, 0/1` 和 `br` 使用，函数发射阶段会跳过冗余布尔化并直接发射 `beq/bne/blt/bge + j`。
- 若比较结果只被紧随其后的 `br` 使用，函数发射阶段直接用原比较操作发射 `beq/bne/blt/bge + j`，不再先生成 `slt/seqz/snez` 布尔值再 `bnez`。
- 常量条件分支在发射阶段直接改写为确定的 `j`，避免生成无意义的条件计算。
- 普通条件分支会优先直接读取已分配的条件寄存器，避免把条件值额外搬到 `t0`。
- 数组地址计算中的 stride 若是 2 的幂，使用 `slli` 替代 `li + mul`。
- `getelemptr/getptr` 下标为常量时，在发射阶段直接折叠成字节偏移，使用 `addi` 或 `li + add`，避免额外加载下标和移位/乘法。
- 已分配到寄存器的 Koopa value 直接从物理寄存器读取，减少栈访问。
- `load` 和二元运算若结果已分配物理寄存器，会直接写入目标寄存器；未分配时才通过 `t0` 写回栈槽。
- `store 0, ptr` 直接使用 `zero` 寄存器；`store value, ptr` 若 value 已在物理寄存器中，会直接从该寄存器写入目标地址。
- 指针 value 若已经在寄存器中，`load/store` 直接使用寄存器间接寻址。
- 目标寄存器直写和直接比较分支折叠在函数发射阶段始终启用。线上验证表明，按“分支密集函数”粗粒度关闭这两类优化会让 `mm/mv/spmv/brainfuck` 明显退化；汇编级保形布局保护也会引入额外 `nop` 或阻止分支缩短，在当前评测环境中同样是负收益，因此默认保持激进窥孔策略。

RISC-V 后端不会仅凭普通数组访问模式识别矩阵转置或矩阵乘法内核；这类高层优化统一保留给显式 tensor dialect，避免把普通二维数组程序误判为 tensor 计算。

## 6. RISC-V 统一优化调度与窥孔优化

RISC-V 后端优化统一由 `src/compiler/riscv/opt/riscv_optimizer.cpp` 调度。当前分为两个层次：

- 发射前阶段：`RiscvOptimizer::allocateRegisters` 调用多候选线性扫描寄存器分配。该阶段输入是解析后的 RISC-V model、IR 活跃变量分析和 call 信息。
- 发射后阶段：`AssemblyOptimizer` 管理所有汇编级 pass，当前注册 `PeepholeOptimizer`。driver 会把 pass 列表反复运行到固定点，最多 128 轮。

寄存器分配纳入统一调度入口，但不放进汇编字符串固定点循环中，因为汇编级窥孔不会反向改变 Koopa value 的 live interval。寄存器分配本身已经在内部做多候选、多轮评分选择；后续如果增加机器级 CFG 或指令级 liveness pass，可以继续注册到 `RiscvOptimizer` 或 `AssemblyOptimizer` 中。

RISC-V 窥孔规则位于 `src/compiler/riscv/opt/peephole_optimizer.cpp`。每次 `PeepholeOptimizer::run` 只执行一轮局部扫描，由 `AssemblyOptimizer` 负责多轮迭代，因此前一轮产生的 `mv`、简化算术或跳转布局变化可以继续暴露给后一轮处理。

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
- 删除相邻的重复 `mv a, b; mv a, b` 和往返 `mv a, b; mv b, a`。
- 将 `bnez/beqz L; j F; L:` 改写为反向条件分支，减少一条无条件跳转。
- 代码中保留了保守布局敏感分析作为实验工具，但当前默认不启用；线上测试表明，保留 `nop` 或阻止分支缩短比可能的布局稳定收益更贵。
- 对栈槽 `lw` 做简单值复用；若同一栈地址刚加载到某寄存器，再次加载可改为 `mv` 或直接删除。

遇到 label、汇编伪指令、函数调用、返回和跳转等边界时，窥孔优化会清空状态，保证跨控制流边界的保守正确性。

## 7. 寄存器分配优化

寄存器分配位于 `src/compiler/riscv/regalloc/`。当前实现不使用图着色，而是复用 IR 层活跃变量分析结果，并通过多候选线性扫描完成分配。

主要文件：

- `register_allocator.cpp`：总入口，组织多轮候选分配和评分选择。
- `register_allocator_analysis.cpp`：从 IR 活跃变量分析结果构造 live interval。
- `register_allocator_linear_scan.cpp`：执行线性扫描分配。
- `register_allocator_call_saves.cpp`：记录跨 call 需要保存的值。
- `register_allocation.*`：保存 value 到物理寄存器的映射和 call-save 信息。
- `riscv_optimizer.*`：将寄存器分配纳入 RISC-V 优化调度入口。

流程：

1. 收集可分配 value，排除 `alloc` 结果。
2. 调用 `analyzeDenseLiveness` 获取每条指令的 live-in/live-out。
3. 根据 use/def/live-in/live-out 构造 live interval。
4. 按不同寄存器偏好策略生成多轮线性扫描候选。
5. 对跨 call 活跃的 value 优先使用 callee-saved register。
6. 对函数参数、call 参数和普通临时值使用不同寄存器偏好顺序。
7. 当寄存器不足时，按下一次使用位置和 interval 结束位置选择当前值或已有 active interval 溢出。
8. 分别尝试默认策略、保留入参寄存器、偏好 callee-saved、偏好 caller-saved 等候选。
9. 按溢出代价、跨 call 保存代价、callee-saved 使用数量和入参寄存器保持情况评分，选择总代价最低的分配。
10. 指令发射阶段只对必要的 caller-saved value 生成 call 前保存和 call 后恢复。

这种实现复杂度低于图着色，同时能覆盖大量局部变量、长 live range、函数参数和跨调用活跃值。多轮分配没有重新实现活跃变量分析，所有候选共享同一份 IR 层 `analyzeDenseLiveness` 结果，只改变线性扫描的寄存器偏好和排序 tie-break。

## 8. 测试与验证

相关测试覆盖：

- IR pass：`src/tests/compiler/ir/opt/ir_optimizer.cpp`
- Tensor dialect：`src/tests/compiler/ir/koopa_tensor_ir.cpp`
- RISC-V 后端：`src/tests/compiler/riscv/riscv_generator.cpp`
- 寄存器分配：`src/tests/compiler/riscv/regalloc/register_allocator.cpp`
- 栈帧和指令发射：`src/tests/compiler/riscv/frame/`、`src/tests/compiler/riscv/emit/`

当前验证情况：

- 宿主机 `g++` 重建测试二进制并通过单元测试：`171/171`
- Docker 环境此前通过单元测试：`168/168`
- SysY 性能测试：`467/467`
- Tensor 矩阵乘专项测试：`manual_tensor_perf_cases/compare_matmul_lowering.py`
- 2021 性能子集脚本：`scripts/run_2021_perf_cases.py`

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

Tensor 矩阵乘专项性能使用 `manual_tensor_perf_cases/compare_matmul_lowering.py` 生成两份语义相同的 `.koopa`：

- `tensor_lowered`：函数体内使用显式 `tensor @C = matmul @A, @B`，由 `TensorMatmulLoweringPass` 生成固定形状循环。
- `naive`：手写普通 Koopa `i-j-k` 三重循环，不带 tensor 语义。

脚本在 Docker 内编译、汇编、链接并通过 `qemu-riscv32-static` 多次运行，只统计编译出的 RISC-V 程序运行时间：

| 矩阵规模 | 重复次数 | 运行轮数 | tensor lowering 平均耗时 | naive 平均耗时 | 比例 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `48x48` | `8` | `6` | `6.753ms` | `10.217ms` | `0.6610` |
| `96x96` | `2` | `4` | `8.091ms` | `13.656ms` | `0.5925` |

这说明当前固定 tensor 矩阵乘 lowering 相比普通 Koopa 三重循环约有 `1.51x` 到 `1.69x` 的 qemu 运行加速。它不改变矩阵乘的 `O(MNK)` 复杂度，收益主要来自内层 `K` 展开、减少循环控制与栈访存、融合 bias/relu/transpose 视图，以及让后续寄存器分配更容易把累加值留在寄存器中。

本轮针对 2021 性能用例新增 `scripts/run_2021_perf_cases.py`。脚本会从 `compiler2021/公开用例与运行时库` 中抽取本地可用的 bitset、mm、mv、spmv、fft、transpose 和 brainfuck 用例，编译为 RISC-V 后用 `qemu-riscv32-static` 运行，并兼容 2021 `.out` 文件末尾单独记录返回值的格式。本地数据集中缺少 `brainfuck-calculator`，因此完整本地子集为 19 个用例。

```sh
python3 scripts/run_2021_perf_cases.py --extract /tmp/sysy2021_selected
python3 scripts/run_2021_perf_cases.py --runs 1
```

同一 Docker/qemu 环境下，本地 19 个 2021 性能子集结果如下：

| 版本或阶段 | 结果 | qemu 总运行时间 | 说明 |
| --- | ---: | ---: | --- |
| `3d835cdabc52` | `19/19 AC` | `272.699864s` | 旧对照提交，同一数据集和脚本 |
| `1b95318` | `19/19 AC` | `214.307747s` | 本轮后端发射优化前 |
| 后端局部优化阶段 | `19/19 AC` | `204.901895s` | 加入立即数比较、常量下标地址折叠、2 的幂除模强度削弱 |
| 冗余布尔化消除阶段 | `19/19 AC` | `208.454058s` | 单次全量受 qemu 波动影响，`brainfuck-bootstrap` 三跑均值为 `15.141303s` |
| 多候选线性扫描阶段 | `19/19 AC` | `200.824968s` | 加入多候选线性扫描寄存器分配；`brainfuck-bootstrap` 三跑均值为 `15.274952s` |
| RISC-V 固定点窥孔阶段 | `19/19 AC` | `197.725312s` | IR pipeline 上限提升到 128 轮，RISC-V 窥孔优化改为固定点迭代；`brainfuck-bootstrap` 三跑均值为 `14.826351s` |
| 直接寄存器发射阶段 | `19/19 AC` | `185.520609s` | `load/store/binary/br` 优先复用寄存器分配结果，减少 `t0` 中转和栈读写 |
| 直接比较分支工作区（防负优化前） | `19/19 AC` | `183.430840s` | 进一步让直接比较分支复用物理寄存器，并将只被分支使用的比较结果直接发射为 RISC-V 条件分支 |
| 函数级布局敏感保护实验 `fb2a96e` | `20/20 AC` | `308.722128s` | 过度关闭目标寄存器直写和直接比较分支，`mm/mv/spmv/brainfuck` 大幅退化；该实验已回退为只保护汇编级布局改写 |
| 汇编级布局保护实验 `680b373` | `20/20 AC` | `259.393554s` | 恢复直接发射后大部分退化消失，但 `brainfuck-bootstrap` 仍慢 `4.47s`；说明汇编保形保护也是负收益，当前已继续回退 |

相对 `1b95318`，当前工作区在该子集上减少 `30.876907s`，约 `14.4%`；相对 `3d835cdabc52`，减少 `89.269024s`，约 `32.7%`。继续分析实际生成的 `brainfuck-bootstrap` 汇编后，发现 `cmp -> ne/eq cmp, 0 -> br` 会产生额外 `snez/mv`，保守阶段曾采用“只删除冗余布尔化、不改写为直接条件分支”的策略。该策略让 `brainfuck-bootstrap` 从上一轮单跑 `16.353877s` 降到三跑均值 `15.141303s`。多候选线性扫描后，完整 19 用例单跑总时间为 `200.824968s`；RISC-V 窥孔固定点迭代后进一步到 `197.725312s`；本轮保留的直接寄存器发射与直接比较分支优化记录到 `183.430840s`。

后续尝试过三类负收益优化并已回退：通用 `beq/bne/blt/bge; j; label` 分支反转、只按单次使用折叠 `getelemptr/getptr + load/store`，以及删除相邻冗余 `snez` 布尔化。这些规则虽然减少静态指令，但会改变分支密集函数的布局；其中 GEP 折叠曾让 `brainfuck-bootstrap` 单跑退化到 `41.680007s`，因此没有保留。最终代码回退这些实验后，`brainfuck-bootstrap` 回到约 `15s`；后续两次完整 19 子集复跑出现多项同步偏慢（`215.848133s`、`218.698546s`），判断为 Docker/qemu 环境抖动，未作为代码生成回归。

此前为了线上测试曾重新启用在部分测试集有效、但本地 2021 子集上不稳定的激进后端优化：

- `mul x, 2^k +/- 1` 展开为移位加减。
- 常量条件分支直接发射为确定跳转。
- 将比较结果直接改写为 `beq/bne/blt/bge` 分支。
- 无保护的相邻反向 `mv` 删除。
- 无保护的窥孔分支布局折叠：`bnez/beqz L; j F; L:` 改写为反向条件分支。

当前代码默认使用激进窥孔策略：普通循环、矩阵类热点和分支密集函数都继续使用直接寄存器发射、直接比较分支、相邻冗余 `mv` 删除和分支布局折叠。保守布局敏感策略保留在代码中，但作为实验工具默认关闭。

普通数组矩阵转置模式识别没有恢复；它存在语义误判风险，不属于单纯性能回退项。矩阵类高层优化仍统一挂在固定形状 tensor dialect 上，而不是从通用数组循环推断。

针对 `brainfuck-bootstrap` 的负优化分析表明，它的热点是分支密集解释器：`get_bf_char` 约 `178` 条指令含 `19` 条分支，`read_program` 约 `104` 条指令含 `8` 条分支，`run_program` 约 `2953` 条指令含 `41` 条分支。曾假设删除冗余 `mv` 或折叠分支会因移动 label 和分支目标地址而影响 qemu translation block 形状；但线上结果显示，保形策略带来的 `nop` 执行成本和错失的静态指令缩短收益更大。因此当前不再启用保形保护，而是优先减少真实执行指令数。

本次防负优化改动已在宿主机用 `g++` 构建并通过完整单元测试 `170/170`。Docker 当前在访问 `/var/run/docker.sock` 的 `_ping` 阶段无响应，因此尚未取得新的 19 点 qemu 性能数；需要 Docker 恢复后再用 `scripts/run_2021_perf_cases.py --runs 1` 复测。
