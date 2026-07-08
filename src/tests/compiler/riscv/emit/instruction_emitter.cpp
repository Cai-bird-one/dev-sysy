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

  const std::string &result_reg = frame.registerFor("%0");
  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  addi " + result_reg + ", " + result_reg +
                         ", 2\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  bnez " + result_reg + ", main_then\n") !=
              std::string::npos);
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

  auto resultRegister = [&](const std::string &value) -> std::string {
    return frame.hasRegisterValue(value) ? frame.registerFor(value) : "t0";
  };
  std::string r0 = resultRegister("%0");
  std::string r1 = resultRegister("%1");
  std::string r2 = resultRegister("%2");
  std::string r3 = resultRegister("%3");
  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  addi " + r0 + ", " + r0 + ", 2\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  addi " + r1 + ", " + r1 + ", -3\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  ori " + r2 + ", " + r2 + ", 4\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  andi " + r3 + ", " + r3 + ", 7\n") !=
              std::string::npos);
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
      "  %2 = mul %1, 3",
      "  %3 = mul %2, 7",
      "  %4 = mul %3, 0",
      "  ret %4",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  auto resultRegister = [&](const std::string &value) -> std::string {
    return frame.hasRegisterValue(value) ? frame.registerFor(value) : "t0";
  };
  std::string r0 = resultRegister("%0");
  std::string r1 = resultRegister("%1");
  std::string r2 = resultRegister("%2");
  std::string r3 = resultRegister("%3");
  const std::string &r4 = frame.registerFor("%4");
  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  slli " + r0 + ", " + r0 + ", 3\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  slli " + r1 + ", " + r1 + ", 4\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  slli t1, " + r2 + ", 1\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  add " + r2 + ", t1, " + r2 + "\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  slli t1, " + r3 + ", 3\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  sub " + r3 + ", t1, " + r3 + "\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  li " + r4 + ", 0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  mul ") == std::string::npos);
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

  auto zeroResultRegister = [&](const std::string &value) -> std::string {
    return frame.hasRegisterValue(value) ? frame.registerFor(value) : "t0";
  };
  std::string r0 = zeroResultRegister("%0");
  std::string r1 = zeroResultRegister("%1");
  std::string r2 = zeroResultRegister("%2");
  std::string r3 = zeroResultRegister("%3");
  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  seqz " + r0 + ", " + r0 + "\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  snez " + r1 + ", " + r1 + "\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  slt " + r2 + ", " + r2 + ", zero\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  slt " + r3 + ", zero, " + r3 + "\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  li t1, 0\n") == std::string::npos);
}

TEST_CASE(instruction_emitter_uses_immediates_for_comparisons) {
  riscv::Function function;
  function.name = "main";
  function.params = {"@x"};
  function.param_types = {"i32"};
  function.instructions = {
      "%entry:",
      "  %0 = lt @x, 100",
      "  %1 = ge @x, 100",
      "  %2 = eq @x, 62",
      "  %3 = ne 62, @x",
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

  auto resultRegister = [&](const std::string &value) -> std::string {
    return frame.hasRegisterValue(value) ? frame.registerFor(value) : "t0";
  };
  std::string r0 = resultRegister("%0");
  std::string r2 = resultRegister("%2");
  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  slti " + r0 + ", " + r0 + ", 100\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  addi " + r2 + ", " + r2 + ", -62\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  li t1, 100\n") == std::string::npos);
  EXPECT_TRUE(riscv.find("  li t1, 62\n") == std::string::npos);
}

TEST_CASE(instruction_emitter_strength_reduces_power_of_two_division) {
  riscv::Function function;
  function.name = "main";
  function.params = {"@x"};
  function.param_types = {"i32"};
  function.instructions = {
      "%entry:",
      "  %0 = div @x, 2",
      "  %1 = mod @x, 2",
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

  const std::string &r1 = frame.registerFor("%1");
  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  srai t1, ") != std::string::npos);
  EXPECT_TRUE(riscv.find("  andi t1, t1, 1\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  srai t1, t1, 1\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  sub " + r1 + ", " + r1 + ", t1\n") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  div ") == std::string::npos);
  EXPECT_TRUE(riscv.find("  rem ") == std::string::npos);
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

TEST_CASE(instruction_emitter_loads_and_stores_colored_registers_directly) {
  riscv::Function function;
  function.name = "main";
  function.params = {"@p", "@v"};
  function.param_types = {"*i32", "i32"};
  function.instructions = {
      "%entry:",
      "  store @v, @p",
      "  store 0, @p",
      "  %0 = load @p",
      "  ret %0",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  const std::string &value_reg = frame.registerFor("@v");
  const std::string &result_reg = frame.registerFor("%0");
  std::string riscv = output.str();

  EXPECT_TRUE(riscv.find("  sw " + value_reg + ", 0(") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  sw zero, 0(") != std::string::npos);
  EXPECT_TRUE(riscv.find("  lw " + result_reg + ", 0(") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("  mv t0, " + value_reg + "\n") ==
              std::string::npos);
  EXPECT_TRUE(riscv.find("  mv " + result_reg + ", t0\n") ==
              std::string::npos);
}

TEST_CASE(instruction_emitter_can_disable_direct_colored_register_emission) {
  riscv::Function function;
  function.name = "main";
  function.params = {"%p", "%v"};
  function.param_types = {"*i32", "i32"};
  function.instructions = {
      "%entry:",
      "  store %v, %p",
      "  store 0, %p",
      "  %0 = load %p",
      "  ret %0",
  };

  riscv::RegisterAllocation allocation;
  allocation.assign("%p", "s1");
  allocation.assign("%v", "s2");
  allocation.assign("%0", "s3");
  riscv::StackFrame frame(function, {}, allocation);
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::InstructionEmitter instructions(function.name, frame, asm_output,
                                         false);

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    instructions.emitInstruction(function.instructions[i], i);
  }

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  mv t0, s2\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  li t0, 0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  sw t0, 0(s1)\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  lw t0, 0(s1)\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  mv s3, t0\n") != std::string::npos);
}
