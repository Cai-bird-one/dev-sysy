#include "compiler/ir/opt/tensor/koopa_tensor_optimize.h"
#include "compiler/ir/tensor/koopa_tensor_ir.h"
#include "compiler/ir/opt/ir_optimizer.h"
#include "compiler/ir/opt/parse/koopa_ir_parser.h"
#include "compiler/riscv/riscv_generator.h"
#include "tests/test_framework.h"

using namespace compiler;

TEST_CASE(koopa_ir_parser_parses_tensor_metadata) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %m = matmul %a, %b : tensor<2x4>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  ret 0\n"
      "}\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);

  EXPECT_EQ(module.tensor.operations.size(), static_cast<size_t>(1));
  EXPECT_EQ(module.tensor.shapes["%m"].str(), std::string("tensor<2x4>"));
}

TEST_CASE(koopa_ir_parser_rejects_bad_tensor_matmul_shape) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<5x4>\n"
      "tensor %m = matmul %a, %b : tensor<2x4>\n";

  bool rejected = false;
  try {
    ir::opt::KoopaIrParser().parse(koopa);
  } catch (const ir::KoopaTensorError &) {
    rejected = true;
  }
  EXPECT_TRUE(rejected);
}

TEST_CASE(koopa_ir_parser_verifies_common_tensor_ops) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<2x3>\n"
      "tensor %row: tensor<1x3>\n"
      "tensor %sum = add %a, %b : tensor<2x3>\n"
      "tensor %flat = reshape %sum : tensor<6>\n"
      "tensor %wide = broadcast %row : tensor<2x3>\n"
      "tensor %t = transpose %sum : tensor<3x2>\n"
      "tensor %relu = relu %wide : tensor<2x3>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);

  EXPECT_EQ(module.tensor.operations.size(), static_cast<size_t>(5));
  EXPECT_EQ(module.tensor.shapes["%flat"].str(), std::string("tensor<6>"));
  EXPECT_EQ(module.tensor.shapes["%t"].str(), std::string("tensor<3x2>"));
}

TEST_CASE(koopa_ir_parser_verifies_fused_tensor_ops) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %bias: tensor<1x4>\n"
      "tensor %x: tensor<2x4>\n"
      "tensor %y: tensor<2x4>\n"
      "tensor %mb = matmul_bias %a, %b, %bias : tensor<2x4>\n"
      "tensor %mr = matmul_relu %a, %b : tensor<2x4>\n"
      "tensor %ar = add_relu %x, %y : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);

  EXPECT_EQ(module.tensor.operations.size(), static_cast<size_t>(3));
  EXPECT_EQ(module.tensor.shapes["%mb"].str(), std::string("tensor<2x4>"));
  EXPECT_EQ(module.tensor.shapes["%mr"].str(), std::string("tensor<2x4>"));
  EXPECT_EQ(module.tensor.shapes["%ar"].str(), std::string("tensor<2x4>"));
}

TEST_CASE(koopa_ir_parser_verifies_transposed_matmul_variants) {
  std::string koopa =
      "tensor %a: tensor<3x2>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %c: tensor<4x3>\n"
      "tensor %x: tensor<2x3>\n"
      "tensor %bias: tensor<1x4>\n"
      "tensor %left = matmul_ta %a, %b : tensor<2x4>\n"
      "tensor %right = matmul_tb %x, %c : tensor<2x4>\n"
      "tensor %both = matmul_bias_relu_ta_tb %a, %c, %bias : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);

  EXPECT_EQ(module.tensor.operations.size(), static_cast<size_t>(3));
  EXPECT_EQ(module.tensor.shapes["%left"].str(), std::string("tensor<2x4>"));
  EXPECT_EQ(module.tensor.shapes["%right"].str(), std::string("tensor<2x4>"));
  EXPECT_EQ(module.tensor.shapes["%both"].str(), std::string("tensor<2x4>"));
}

TEST_CASE(koopa_ir_parser_rejects_bad_tensor_reshape_and_broadcast) {
  std::string bad_reshape =
      "tensor %a: tensor<2x3>\n"
      "tensor %bad = reshape %a : tensor<5>\n";
  std::string bad_broadcast =
      "tensor %a: tensor<2x3>\n"
      "tensor %bad = broadcast %a : tensor<4x3>\n";

  bool rejected_reshape = false;
  try {
    ir::opt::KoopaIrParser().parse(bad_reshape);
  } catch (const ir::KoopaTensorError &) {
    rejected_reshape = true;
  }

  bool rejected_broadcast = false;
  try {
    ir::opt::KoopaIrParser().parse(bad_broadcast);
  } catch (const ir::KoopaTensorError &) {
    rejected_broadcast = true;
  }

  EXPECT_TRUE(rejected_reshape);
  EXPECT_TRUE(rejected_broadcast);
}

TEST_CASE(koopa_tensor_ir_optimizes_redundant_tensor_ops) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %same = reshape %a : tensor<2x3>\n"
      "tensor %t0 = transpose %same : tensor<3x2>\n"
      "tensor %t1 = transpose %t0 : tensor<2x3>\n"
      "tensor %out = reshape %t1 : tensor<3x2>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);
  ir::KoopaTensorInfo optimized = ir::opt::optimizeTensorInfo(module.tensor);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(1));
  EXPECT_TRUE(optimized.operations[0].kind == ir::KoopaTensorOpKind::Reshape);
  EXPECT_EQ(optimized.operations[0].result, std::string("%out"));
  EXPECT_EQ(optimized.operations[0].operands[0], std::string("%a"));
}

TEST_CASE(koopa_tensor_ir_fuses_matmul_bias_relu) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %bias: tensor<1x4>\n"
      "tensor %bb = broadcast %bias : tensor<2x4>\n"
      "tensor %m = matmul %a, %b : tensor<2x4>\n"
      "tensor %sum = add %m, %bb : tensor<2x4>\n"
      "tensor %out = relu %sum : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);
  ir::KoopaTensorInfo optimized = ir::opt::optimizeTensorInfo(module.tensor);
  std::string formatted = ir::formatTensorInfo(optimized);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(1));
  EXPECT_TRUE(optimized.operations[0].kind ==
              ir::KoopaTensorOpKind::MatmulBiasRelu);
  EXPECT_TRUE(formatted.find("matmul_bias_relu %a, %b, %bias") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %bb = broadcast") == std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %sum = add") == std::string::npos);
}

TEST_CASE(koopa_tensor_ir_fuses_matmul_bias_and_matmul_relu) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %c: tensor<3x4>\n"
      "tensor %bias: tensor<1x4>\n"
      "tensor %bb = broadcast %bias : tensor<2x4>\n"
      "tensor %m = matmul %a, %b : tensor<2x4>\n"
      "tensor %with_bias = add %bb, %m : tensor<2x4>\n"
      "tensor %m2 = matmul %a, %c : tensor<2x4>\n"
      "tensor %with_relu = relu %m2 : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);
  ir::KoopaTensorInfo optimized = ir::opt::optimizeTensorInfo(module.tensor);
  std::string formatted = ir::formatTensorInfo(optimized);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(2));
  EXPECT_TRUE(formatted.find(
                  "tensor %with_bias = matmul_bias %a, %b, %bias") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %with_relu = matmul_relu %a, %c") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %bb = broadcast") == std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %m = matmul") == std::string::npos);
}

TEST_CASE(koopa_tensor_ir_absorbs_transposes_into_matmul) {
  std::string koopa =
      "tensor %a: tensor<3x2>\n"
      "tensor %b: tensor<4x3>\n"
      "tensor %ta = transpose %a : tensor<2x3>\n"
      "tensor %tb = transpose %b : tensor<3x4>\n"
      "tensor %out = matmul %ta, %tb : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);
  ir::KoopaTensorInfo optimized = ir::opt::optimizeTensorInfo(module.tensor);
  std::string formatted = ir::formatTensorInfo(optimized);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(1));
  EXPECT_TRUE(formatted.find("tensor %out = matmul_ta_tb %a, %b") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("transpose") == std::string::npos);
}

TEST_CASE(koopa_tensor_ir_fuses_transposed_matmul_bias_relu) {
  std::string koopa =
      "tensor %a: tensor<3x2>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %bias: tensor<1x4>\n"
      "tensor %ta = transpose %a : tensor<2x3>\n"
      "tensor %bb = broadcast %bias : tensor<2x4>\n"
      "tensor %m = matmul %ta, %b : tensor<2x4>\n"
      "tensor %sum = add %m, %bb : tensor<2x4>\n"
      "tensor %out = relu %sum : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);
  ir::KoopaTensorInfo optimized = ir::opt::optimizeTensorInfo(module.tensor);
  std::string formatted = ir::formatTensorInfo(optimized);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(1));
  EXPECT_TRUE(formatted.find("tensor %out = matmul_bias_relu_ta %a, %b, %bias") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("transpose") == std::string::npos);
  EXPECT_TRUE(formatted.find("broadcast") == std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %sum = add") == std::string::npos);
}

TEST_CASE(koopa_tensor_ir_eliminates_duplicate_tensor_expressions) {
  std::string koopa =
      "tensor %x: tensor<2x4>\n"
      "tensor %bias: tensor<1x4>\n"
      "tensor %bb0 = broadcast %bias : tensor<2x4>\n"
      "tensor %bb1 = broadcast %bias : tensor<2x4>\n"
      "tensor %sum0 = add %x, %bb0 : tensor<2x4>\n"
      "tensor %sum1 = add %bb1, %x : tensor<2x4>\n"
      "tensor %out = relu %sum1 : tensor<2x4>\n";

  ir::opt::IrModule module = ir::opt::KoopaIrParser().parse(koopa);
  ir::KoopaTensorInfo optimized = ir::opt::optimizeTensorInfo(module.tensor);
  std::string formatted = ir::formatTensorInfo(optimized);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(2));
  EXPECT_TRUE(formatted.find("tensor %bb0 = broadcast %bias") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %out = add_relu %bb0, %x") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %bb1") == std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %sum0") == std::string::npos);
  EXPECT_TRUE(formatted.find("tensor %sum1") == std::string::npos);
}

TEST_CASE(koopa_tensor_dialect_does_not_break_existing_pipeline) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  ret 0\n"
      "}\n";

  std::string optimized = ir::opt::IrOptimizer().optimize(koopa);
  std::string riscv = riscv::RiscvGenerator().generate(optimized);

  EXPECT_TRUE(optimized.find("tensor %a: tensor<2x3>") !=
              std::string::npos);
  EXPECT_TRUE(optimized.find("ret 0") != std::string::npos);
  EXPECT_TRUE(riscv.find("li a0, 0") != std::string::npos);
}

TEST_CASE(ir_optimizer_preserves_optimized_tensor_dialect) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<3x4>\n"
      "tensor %bias: tensor<2x4>\n"
      "tensor %m = matmul %a, %b : tensor<2x4>\n"
      "tensor %sum = add %m, %bias : tensor<2x4>\n"
      "tensor %out = relu %sum : tensor<2x4>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  ret 0\n"
      "}\n";

  std::string optimized = ir::opt::IrOptimizer().optimize(koopa);

  EXPECT_TRUE(optimized.find("tensor %out = matmul_bias_relu %a, %b, %bias") !=
              std::string::npos);
  EXPECT_TRUE(optimized.find("tensor %sum = add") == std::string::npos);
}

TEST_CASE(ir_optimizer_lowers_function_local_tensor_matmul) {
  std::string koopa =
      "tensor %a: tensor<2x3>\n"
      "tensor %b: tensor<3x2>\n"
      "tensor %c: tensor<2x2>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  %a = alloc [[i32, 3], 2]\n"
      "  %b = alloc [[i32, 2], 3]\n"
      "  %c = alloc [[i32, 2], 2]\n"
      "  %a0 = getelemptr %a, 0\n"
      "  %a00 = getelemptr %a0, 0\n"
      "  store 2, %a00\n"
      "  %a01 = getelemptr %a0, 1\n"
      "  store 3, %a01\n"
      "  %a02 = getelemptr %a0, 2\n"
      "  store 4, %a02\n"
      "  %a1 = getelemptr %a, 1\n"
      "  %a10 = getelemptr %a1, 0\n"
      "  store 5, %a10\n"
      "  %a11 = getelemptr %a1, 1\n"
      "  store 6, %a11\n"
      "  %a12 = getelemptr %a1, 2\n"
      "  store 7, %a12\n"
      "  %b0 = getelemptr %b, 0\n"
      "  %b00 = getelemptr %b0, 0\n"
      "  store 11, %b00\n"
      "  %b01 = getelemptr %b0, 1\n"
      "  store 13, %b01\n"
      "  %b1 = getelemptr %b, 1\n"
      "  %b10 = getelemptr %b1, 0\n"
      "  store 17, %b10\n"
      "  %b11 = getelemptr %b1, 1\n"
      "  store 19, %b11\n"
      "  %b2 = getelemptr %b, 2\n"
      "  %b20 = getelemptr %b2, 0\n"
      "  store 23, %b20\n"
      "  %b21 = getelemptr %b2, 1\n"
      "  store 29, %b21\n"
      "  tensor %c = matmul %a, %b : tensor<2x2>\n"
      "  %crow = getelemptr %c, 1\n"
      "  %celem = getelemptr %crow, 1\n"
      "  %out = load %celem\n"
      "  ret %out\n"
      "}\n";

  std::string optimized = ir::opt::IrOptimizer().optimize(koopa);
  std::string riscv = riscv::RiscvGenerator().generate(optimized);

  EXPECT_TRUE(optimized.find("tensor %c = matmul") == std::string::npos);
  EXPECT_TRUE(optimized.find("%tensor_mm0_unroll_cell:") !=
              std::string::npos);
  EXPECT_TRUE(optimized.find("%tensor_mm0_unroll_j_cond:") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("tensor_mm0_unroll_cell") != std::string::npos);
}

TEST_CASE(koopa_tensor_ir_formats_tensor_dialect) {
  std::string shape = ir::formatTensorShapeDecl("%x", {{1, 4}});
  std::string op = ir::formatTensorOpDecl(
      {ir::KoopaTensorOpKind::Relu, "%y", {"%x"}, {{1, 4}}});

  EXPECT_EQ(shape, std::string("tensor %x: tensor<1x4>"));
  EXPECT_EQ(op, std::string("tensor %y = relu %x : tensor<1x4>"));
}
