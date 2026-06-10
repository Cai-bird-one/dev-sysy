#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/emit/instruction_emitter.h"
#include "compiler/riscv/frame/stack_frame.h"
#include "tests/test_framework.h"

#include <sstream>

using namespace compiler;

TEST_CASE(instruction_emitter_emits_labels_branches_calls_and_binary_ops) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  br %0, %then, %else",
      "%then:",
      "  %1 = call @callee(%0)",
      "  ret %1",
  };

  riscv::StackFrame frame(function, {});
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (const std::string &line : function.instructions) {
    instructions.emitInstruction(line);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  add t0, t0, t1\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  bnez t0, main_then\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  j main_else\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("main_then:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  call callee\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  ret\n") != std::string::npos);
}
