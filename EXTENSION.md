# Tensor IR 说明

本文档只描述当前编译器新增的 Koopa tensor IR dialect。优化与 lowering 相关内容统一放在 `OPTIMIZATION.md`。

## 1. 设计目标

Tensor IR 的目标是在 SysY 前端加入一个和 `int` 同级的 `tensor` 基本类型，并在 Koopa IR 中保留 tensor 形状和 tensor 操作元信息。

当前实现采用 `tensor` 开头的 dialect 行承载 tensor IR 信息：

- Lexer 会把 `tensor` 识别为 `TENSOR` 关键字。
- Parser 将 `tensor` 纳入 `BType`，因此语法树中的声明类型可以是 `int` 或 `tensor`。
- Koopa 生成阶段把 `tensor a[2][3];` 编译为普通数组存储加 tensor 形状声明。
- Tensor 信息使用独立的 `tensor ...` IR 行，普通注释不参与 tensor 表达。
- Koopa IR parser 会把 `tensor ...` 行解析到 `IrModule::tensor` 中。
- 普通 scalar pass 不把 tensor 行当作标量指令处理。
- RISC-V 后端会忽略仍作为元信息保留的 tensor dialect 行。

相关实现文件：

- `src/compiler/ir/tensor/koopa_tensor_ir.h`
- `src/compiler/ir/tensor/koopa_tensor_ir.cpp`
- `src/compiler/ir/tensor/koopa_tensor_verify.h`
- `src/compiler/ir/tensor/koopa_tensor_verify.cpp`
- `src/compiler/lexer/token_rules.cpp`
- `src/compiler/parser/grammar_rules.cpp`
- `src/compiler/ir/opt/model/ir_module.h`
- `src/compiler/ir/opt/parse/koopa_ir_parser.cpp`

## 2. 源语言格式

Tensor 声明在源语言中使用 `tensor` 关键字，维度沿用现有数组维度语法：

```c
tensor a[2][3];
```

当前 tensor 声明必须至少带一维；`tensor a;` 会被 IR 生成阶段拒绝，因为没有可记录的 tensor 形状。

对应的 Koopa IR 会包含普通数组存储和 tensor 形状声明，例如局部声明会生成：

```koopa
%a = alloc [[i32, 3], 2]
tensor %a: tensor<2x3>
```

## 3. IR 格式

Tensor 形状声明：

```koopa
tensor %a: tensor<2x3>
```

Tensor 操作声明：

```koopa
tensor %m = matmul %a, %b : tensor<2x4>
```

格式要求：

- Tensor dialect 行必须以 `tensor ` 开头。
- 形状格式为 `tensor<d0xd1x...>`。
- 每一维必须是正整数。
- tensor 至少有一维，不支持 rank-0。
- 操作结果和操作数使用 Koopa value 名称，例如 `%a`、`%m`。
- 操作数必须在之前的 tensor 声明中已经声明形状。

## 4. 支持的操作

当前 tensor IR 支持 10 类基础操作，并为矩阵乘法族提供转置视图变体：

| 操作 | 操作数数量 | 形状规则 |
| --- | --- | --- |
| `add` | 2 | 两个输入形状必须完全相同，输出形状也必须相同 |
| `add_relu` | 2 | `add` 与 `relu` 的融合形式；两个输入和输出形状必须完全相同 |
| `broadcast` | 1 | 输入可按从右对齐的广播规则扩展到输出形状；输入维度必须等于目标维度或为 `1` |
| `matmul` | 2 | 两个输入都必须是 rank-2 矩阵；左矩阵为 `MxK`，右矩阵为 `KxN`，输出必须是 `MxN` |
| `matmul_bias` | 3 | `matmul` 与 bias add 的融合形式；第三个 bias 必须可广播到输出形状 |
| `matmul_bias_relu` | 3 | 复合操作；前两个输入满足 `matmul` 规则，第三个 bias 必须可广播到输出形状 |
| `matmul_relu` | 2 | `matmul` 与 `relu` 的融合形式；形状规则同 `matmul` |
| `relu` | 1 | 输出形状必须与输入形状相同 |
| `reshape` | 1 | 输入和输出元素总数必须相同 |
| `transpose` | 1 | 当前仅支持 rank-2 输入，输出必须交换两个维度 |

矩阵乘法族还支持后缀 `_ta`、`_tb`、`_ta_tb`，表示左操作数、右操作数或两个操作数按转置视图参与计算，而不是先物化一个 `transpose` 结果。例如：

- `matmul_ta %a, %b` 表示 `%a^T * %b`。
- `matmul_tb %a, %b` 表示 `%a * %b^T`。
- `matmul_bias_relu_ta_tb %a, %b, %bias` 表示 `%a^T * %b^T + bias` 后再做 `relu`。

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

## 5. 校验规则

解析 tensor dialect 时会同步做静态校验：

- 未知 tensor value 会报错。
- 未知 tensor 操作名会报错。
- 操作数数量不匹配会报错。
- `matmul` 内维不一致会报错。
- `matmul_*_ta/tb` 会按转置后的有效形状检查内维和输出形状。
- `reshape` 改变元素总数会报错。
- `transpose` 非 rank-2 输入会报错。
- `add` 两侧形状不同会报错。
- `add_relu` 两侧形状不同会报错。
- `broadcast` 不满足广播规则会报错。
- `matmul_bias` / `matmul_bias_relu` 的 bias 不能广播到输出形状会报错。
- `relu` 输出形状不等于输入形状会报错。

这些错误统一以 `KoopaTensorError` 抛出。

## 6. 当前边界

当前 tensor IR 仍有一些明确边界：

- Tensor 操作没有 dtype 系统，当前只记录形状和操作关系。
- Tensor 函数返回值暂未开放；tensor 函数参数目前要求使用数组参数形式。
- `transpose` 只支持 rank-2。
- `add` 不做隐式 broadcasting，若需要广播必须显式写 `broadcast`。
- Tensor dialect 不会从普通数组循环中反推矩阵语义；需要 tensor 语义时必须显式写入 tensor 声明或 tensor 操作。
- 哪些 tensor 操作会被优化或 lowering 到普通 Koopa/RISC-V，见 `OPTIMIZATION.md`。
