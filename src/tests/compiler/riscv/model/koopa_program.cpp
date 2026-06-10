#include "compiler/riscv/model/koopa_program.h"
#include "tests/test_framework.h"

using namespace compiler;

TEST_CASE(koopa_program_parses_globals_functions_and_calls) {
  riscv::Program program = riscv::parseProgram(
      "global @g = alloc [i32, 3], {1, 2, 0}\n"
      "\n"
      "fun @callee(@x: i32): i32 {\n"
      "%entry:\n"
      "  ret @x\n"
      "}\n"
      "\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  %0 = call @callee(7)\n"
      "  ret %0\n"
      "}\n");

  EXPECT_EQ(program.globals.size(), 1u);
  EXPECT_EQ(program.globals[0].name, "g");
  EXPECT_EQ(program.globals[0].initial_values.size(), 3u);
  EXPECT_EQ(program.globals[0].initial_values[1], 2);
  EXPECT_EQ(program.globals[0].dimensions.size(), 1u);
  EXPECT_EQ(program.globals[0].dimensions[0], 3);

  EXPECT_EQ(program.functions.size(), 2u);
  EXPECT_EQ(program.functions[0].name, "callee");
  EXPECT_EQ(program.functions[0].params.size(), 1u);
  EXPECT_EQ(program.functions[0].params[0], "@x");
  EXPECT_EQ(program.functions[1].instructions.size(), 3u);

  riscv::CallInstruction call =
      riscv::parseCallInstruction("  %0 = call @callee(7, @g)");
  EXPECT_TRUE(call.has_result);
  EXPECT_EQ(call.result, "%0");
  EXPECT_EQ(call.callee, "@callee");
  EXPECT_EQ(call.args.size(), 2u);
  EXPECT_EQ(call.args[1], "@g");
}
