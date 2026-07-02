# Tensor 扩展说明

本文档只描述当前编译器新增的 Koopa IR tensor 注释扩展。优化相关内容统一放在 `OPTIMIZATION.md`。

## 1. 设计目标

Tensor 扩展的目标是在不改变 SysY 源语言、不破坏 Koopa IR 原有语法的前提下，为 IR 文本附加 tensor 形状和 tensor 操作元信息。

当前实现采用注释形式承载扩展信息：

- 普通 Koopa parser 可以安全忽略这些注释。
- RISC-V 后端不会把 tensor 注释当作实际指令处理。
- IR 优化器可以单独解析、校验和格式化这些 tensor 元信息。

相关实现文件：

- `src/compiler/ir/koopa_tensor_extension.h`
- `src/compiler/ir/koopa_tensor_extension.cpp`
- `src/compiler/ir/koopa_tensor_verify.cpp`
- `src/compiler/ir/koopa_tensor_optimize.cpp`

## 2. 注释格式

Tensor 形状声明：

```koopa
// tensor %a: tensor<2x3>
```

Tensor 操作声明：

```koopa
// tensor %m = matmul %a, %b : tensor<2x4>
```

格式要求：

- 注释必须以 `// tensor ` 开头。
- 形状格式为 `tensor<d0xd1x...>`。
- 每一维必须是正整数。
- tensor 至少有一维，不支持 rank-0。
- 操作结果和操作数使用 Koopa value 名称，例如 `%a`、`%m`。
- 操作数必须在之前的 tensor 注释中已经声明形状。

## 3. 支持的操作

当前 tensor 扩展支持 7 类操作：

| 操作 | 操作数数量 | 形状规则 |
| --- | --- | --- |
| `add` | 2 | 两个输入形状必须完全相同，输出形状也必须相同 |
| `broadcast` | 1 | 输入可按从右对齐的广播规则扩展到输出形状；输入维度必须等于目标维度或为 `1` |
| `matmul` | 2 | 两个输入都必须是 rank-2 矩阵；左矩阵为 `MxK`，右矩阵为 `KxN`，输出必须是 `MxN` |
| `matmul_bias_relu` | 3 | 复合操作；前两个输入满足 `matmul` 规则，第三个 bias 必须可广播到输出形状 |
| `relu` | 1 | 输出形状必须与输入形状相同 |
| `reshape` | 1 | 输入和输出元素总数必须相同 |
| `transpose` | 1 | 当前仅支持 rank-2 输入，输出必须交换两个维度 |

示例：

```koopa
// tensor %a: tensor<2x3>
// tensor %b: tensor<3x4>
// tensor %bias: tensor<1x4>
// tensor %bb = broadcast %bias : tensor<2x4>
// tensor %m = matmul %a, %b : tensor<2x4>
// tensor %sum = add %m, %bb : tensor<2x4>
// tensor %out = relu %sum : tensor<2x4>
```

## 4. 校验规则

解析 tensor 注释时会同步做静态校验：

- 未知 tensor value 会报错。
- 未知 tensor 操作名会报错。
- 操作数数量不匹配会报错。
- `matmul` 内维不一致会报错。
- `reshape` 改变元素总数会报错。
- `transpose` 非 rank-2 输入会报错。
- `add` 两侧形状不同会报错。
- `broadcast` 不满足广播规则会报错。
- `relu` 输出形状不等于输入形状会报错。

这些错误统一以 `KoopaTensorError` 抛出。

## 5. 当前边界

当前 tensor 扩展仍有一些明确边界：

- Tensor 扩展是注释级元信息扩展，不会把 tensor 操作 lowering 成实际循环代码或专用指令。
- Tensor 操作没有 dtype 系统，当前只记录形状和操作关系。
- `transpose` 只支持 rank-2。
- `add` 不做隐式 broadcasting，若需要广播必须显式写 `broadcast`。
- `matmul_bias_relu` 允许作为注释直接出现并校验，但不会在后端生成专用 tensor 指令。
