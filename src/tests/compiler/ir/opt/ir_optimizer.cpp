#include "compiler/ir/opt/ir_optimizer.h"
#include "compiler/riscv/riscv_generator.h"
#include "tests/test_framework.h"

#include <string>

using namespace compiler;

TEST_CASE(ir_optimizer_folds_constants_and_removes_dead_result) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = add 1, 2\n"
                         "  ret %0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret 3") != std::string::npos);
}

TEST_CASE(ir_optimizer_simplifies_algebraic_identity) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = load %a\n"
                         "  %1 = add %0, 0\n"
                         "  ret %1\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%1 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret %0") != std::string::npos);
}

TEST_CASE(ir_optimizer_removes_unreachable_instructions_after_terminator) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  ret 1\n"
                         "  %0 = add 2, 3\n"
                         "%next:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("%next:\n") != std::string::npos);
}

TEST_CASE(ir_optimizer_simplifies_constant_branches) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  br 0, %then, %else\n"
                         "%then:\n"
                         "  ret 1\n"
                         "%else:\n"
                         "  ret 2\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("br 0") == std::string::npos);
  EXPECT_TRUE(optimized.find("jump %else") != std::string::npos);
}

TEST_CASE(ir_optimizer_threads_trivial_jumps) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  jump %mid\n"
                         "%mid:\n"
                         "  jump %exit\n"
                         "%exit:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("jump %mid") == std::string::npos);
  EXPECT_TRUE(optimized.find("jump %exit") != std::string::npos);
}

TEST_CASE(ir_optimizer_eliminates_basic_block_common_subexpressions) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@a: i32, @b: i32): i32 {\n"
                         "%entry:\n"
                         "  %0 = add @a, @b\n"
                         "  %1 = add @b, @a\n"
                         "  %2 = mul %1, 2\n"
                         "  ret %2\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%1 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("%2 = mul %0, 2") != std::string::npos);
}

TEST_CASE(ir_optimizer_forwards_local_scalar_loads) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@x: i32): i32 {\n"
                         "%entry:\n"
                         "  %a = alloc i32\n"
                         "  store @x, %a\n"
                         "  %0 = load %a\n"
                         "  %1 = add %0, 1\n"
                         "  ret %1\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = load %a") == std::string::npos);
  EXPECT_TRUE(optimized.find("%1 = add @x, 1") != std::string::npos);
}

TEST_CASE(ir_optimizer_output_still_feeds_riscv_generator) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = mul 7, 6\n"
                         "  ret %0\n"
                         "}\n");

  riscv::RiscvGenerator generator;
  std::string assembly = generator.generate(optimized);
  EXPECT_TRUE(assembly.find("li a0, 42") != std::string::npos);
}

TEST_CASE(ir_optimizer_preserves_call_argument_values) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @id(@id_x: i32): i32 {\n"
                         "%entry:\n"
                         "  ret @id_x\n"
                         "}\n"
                         "\n"
                         "fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = load @g\n"
                         "  %1 = add %0, 1\n"
                         "  %2 = call @id(%1)\n"
                         "  ret %2\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%1 = add %0, 1") != std::string::npos);
  EXPECT_TRUE(optimized.find("%2 = call @id(%1)") != std::string::npos);
}

TEST_CASE(ir_optimizer_keeps_call_commas_when_replacing_operands) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @sum(@a: i32, @b: i32): i32 {\n"
                         "%entry:\n"
                         "  ret @a\n"
                         "}\n"
                         "\n"
                         "fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = add 1, 2\n"
                         "  call @sum(%0, %0)\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("call @sum(3, 3)") != std::string::npos);
}
