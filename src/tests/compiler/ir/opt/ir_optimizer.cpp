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
