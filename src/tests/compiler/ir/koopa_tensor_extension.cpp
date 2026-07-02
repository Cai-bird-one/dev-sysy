#include "compiler/ir/koopa_tensor_extension.h"
#include "compiler/ir/opt/ir_optimizer.h"
#include "compiler/riscv/riscv_generator.h"
#include "tests/test_framework.h"

using namespace compiler;

TEST_CASE(koopa_tensor_extension_parses_shape_metadata) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %b: tensor<3x4>\n"
      "// tensor %m = matmul %a, %b : tensor<2x4>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  ret 0\n"
      "}\n";

  ir::KoopaTensorInfo info = ir::KoopaTensorExtension().parse(koopa);

  EXPECT_EQ(info.operations.size(), static_cast<size_t>(1));
  EXPECT_EQ(info.shapes["%m"].str(), std::string("tensor<2x4>"));
}

TEST_CASE(koopa_tensor_extension_rejects_bad_matmul_shape) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %b: tensor<5x4>\n"
      "// tensor %m = matmul %a, %b : tensor<2x4>\n";

  bool rejected = false;
  try {
    ir::KoopaTensorExtension().verify(koopa);
  } catch (const ir::KoopaTensorError &) {
    rejected = true;
  }
  EXPECT_TRUE(rejected);
}

TEST_CASE(koopa_tensor_extension_verifies_common_tensor_ops) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %b: tensor<2x3>\n"
      "// tensor %row: tensor<1x3>\n"
      "// tensor %sum = add %a, %b : tensor<2x3>\n"
      "// tensor %flat = reshape %sum : tensor<6>\n"
      "// tensor %wide = broadcast %row : tensor<2x3>\n"
      "// tensor %t = transpose %sum : tensor<3x2>\n"
      "// tensor %relu = relu %wide : tensor<2x3>\n";

  ir::KoopaTensorInfo info = ir::KoopaTensorExtension().parse(koopa);

  EXPECT_EQ(info.operations.size(), static_cast<size_t>(5));
  EXPECT_EQ(info.shapes["%flat"].str(), std::string("tensor<6>"));
  EXPECT_EQ(info.shapes["%t"].str(), std::string("tensor<3x2>"));
}

TEST_CASE(koopa_tensor_extension_rejects_bad_reshape_and_broadcast) {
  std::string bad_reshape =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %bad = reshape %a : tensor<5>\n";
  std::string bad_broadcast =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %bad = broadcast %a : tensor<4x3>\n";

  bool rejected_reshape = false;
  try {
    ir::KoopaTensorExtension().verify(bad_reshape);
  } catch (const ir::KoopaTensorError &) {
    rejected_reshape = true;
  }

  bool rejected_broadcast = false;
  try {
    ir::KoopaTensorExtension().verify(bad_broadcast);
  } catch (const ir::KoopaTensorError &) {
    rejected_broadcast = true;
  }

  EXPECT_TRUE(rejected_reshape);
  EXPECT_TRUE(rejected_broadcast);
}

TEST_CASE(koopa_tensor_extension_optimizes_redundant_tensor_ops) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %same = reshape %a : tensor<2x3>\n"
      "// tensor %t0 = transpose %same : tensor<3x2>\n"
      "// tensor %t1 = transpose %t0 : tensor<2x3>\n"
      "// tensor %out = reshape %t1 : tensor<3x2>\n";

  ir::KoopaTensorInfo optimized = ir::KoopaTensorExtension().optimize(koopa);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(1));
  EXPECT_TRUE(optimized.operations[0].kind == ir::KoopaTensorOpKind::Reshape);
  EXPECT_EQ(optimized.operations[0].result, std::string("%out"));
  EXPECT_EQ(optimized.operations[0].operands[0], std::string("%a"));
}

TEST_CASE(koopa_tensor_extension_fuses_matmul_bias_relu) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %b: tensor<3x4>\n"
      "// tensor %bias: tensor<1x4>\n"
      "// tensor %bb = broadcast %bias : tensor<2x4>\n"
      "// tensor %m = matmul %a, %b : tensor<2x4>\n"
      "// tensor %sum = add %m, %bb : tensor<2x4>\n"
      "// tensor %out = relu %sum : tensor<2x4>\n";

  ir::KoopaTensorExtension extension;
  ir::KoopaTensorInfo optimized = extension.optimize(koopa);
  std::string formatted = extension.format(optimized);

  EXPECT_EQ(optimized.operations.size(), static_cast<size_t>(2));
  EXPECT_TRUE(optimized.operations[1].kind ==
              ir::KoopaTensorOpKind::MatmulBiasRelu);
  EXPECT_TRUE(formatted.find("matmul_bias_relu %a, %b, %bb") !=
              std::string::npos);
  EXPECT_TRUE(formatted.find("// tensor %sum = add") == std::string::npos);
}

TEST_CASE(koopa_tensor_comments_do_not_break_existing_pipeline) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  ret 0\n"
      "}\n";

  std::string optimized = ir::opt::IrOptimizer().optimize(koopa);
  std::string riscv = riscv::RiscvGenerator().generate(optimized);

  EXPECT_TRUE(optimized.find("// tensor %a: tensor<2x3>") !=
              std::string::npos);
  EXPECT_TRUE(optimized.find("ret 0") != std::string::npos);
  EXPECT_TRUE(riscv.find("li a0, 0") != std::string::npos);
}

TEST_CASE(ir_optimizer_preserves_optimized_tensor_annotations) {
  std::string koopa =
      "// tensor %a: tensor<2x3>\n"
      "// tensor %b: tensor<3x4>\n"
      "// tensor %bias: tensor<2x4>\n"
      "// tensor %m = matmul %a, %b : tensor<2x4>\n"
      "// tensor %sum = add %m, %bias : tensor<2x4>\n"
      "// tensor %out = relu %sum : tensor<2x4>\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  ret 0\n"
      "}\n";

  std::string optimized = ir::opt::IrOptimizer().optimize(koopa);

  EXPECT_TRUE(optimized.find("// tensor %out = matmul_bias_relu %a, %b, %bias") !=
              std::string::npos);
  EXPECT_TRUE(optimized.find("// tensor %sum = add") == std::string::npos);
}

TEST_CASE(koopa_tensor_extension_formats_annotations) {
  ir::KoopaTensorExtension extension;
  std::string shape = extension.annotateShape("%x", {{1, 4}});
  std::string op = extension.annotateOp(
      {ir::KoopaTensorOpKind::Relu, "%y", {"%x"}, {{1, 4}}});

  EXPECT_EQ(shape, std::string("// tensor %x: tensor<1x4>"));
  EXPECT_EQ(op, std::string("// tensor %y = relu %x : tensor<1x4>"));
}
