#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/emit/instruction_emitter.h"
#include "compiler/riscv/frame/stack_frame.h"
#include "compiler/riscv/regalloc/register_allocator.h"
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

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  addi t0, t0, 2\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  bnez t0, main_then\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  j main_else\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("main_then:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  call callee\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  ret\n") != std::string::npos);
}

TEST_CASE(instruction_emitter_uses_immediate_binary_instructions) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  %1 = sub %0, 3",
      "  %2 = or 4, %1",
      "  %3 = and %2, 7",
      "  ret %3",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  addi t0, t0, 2\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  addi t0, t0, -3\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  ori t0, t0, 4\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  andi t0, t0, 7\n") != std::string::npos);
}

TEST_CASE(instruction_emitter_strength_reduces_power_of_two_multiply) {
  riscv::Function function;
  function.name = "main";
  function.params = {"@x"};
  function.param_types = {"i32"};
  function.instructions = {
      "%entry:",
      "  %0 = mul @x, 8",
      "  %1 = mul 16, %0",
      "  %2 = mul %1, 0",
      "  ret %2",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  slli t0, t0, 3\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  slli t0, t0, 4\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  li t0, 0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  mul t0, t0, t1\n") == std::string::npos);
}

TEST_CASE(instruction_emitter_uses_zero_register_for_zero_comparisons) {
  riscv::Function function;
  function.name = "main";
  function.params = {"@x"};
  function.param_types = {"i32"};
  function.instructions = {
      "%entry:",
      "  %0 = eq @x, 0",
      "  %1 = ne 0, @x",
      "  %2 = lt @x, 0",
      "  %3 = ge 0, @x",
      "  ret %3",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  seqz t0, t0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  snez t0, t0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  slt t0, t0, zero\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  slt t0, zero, t0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  li t1, 0\n") == std::string::npos);
}

TEST_CASE(instruction_emitter_uses_colored_registers_for_temporaries) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  %1 = mul %0, 3",
      "  ret %1",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  mv ") != std::string::npos);
  EXPECT_TRUE(riscv.find("  sw t0, ") == std::string::npos);
  EXPECT_TRUE(riscv.find("  lw t0, ") == std::string::npos);
}
