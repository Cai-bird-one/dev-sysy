#include "compiler/riscv/regalloc/register_allocator.h"
#include "compiler/riscv/regalloc/program_register_allocator.h"
#include "tests/test_framework.h"

using namespace compiler;

TEST_CASE(register_allocator_colors_non_calling_scalar_temporaries) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  %1 = mul %0, 3",
      "  ret %1",
  };

  riscv::RegisterAllocation allocation =
      riscv::RegisterAllocator().allocate(function);

  EXPECT_TRUE(allocation.hasRegister("%0"));
  EXPECT_TRUE(allocation.hasRegister("%1"));
}

TEST_CASE(register_allocator_saves_values_live_across_calls) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  %1 = call @f()",
      "  %2 = add %0, %1",
      "  ret %2",
  };

  riscv::RegisterAllocation allocation =
      riscv::RegisterAllocator().allocate(function);

  EXPECT_TRUE(allocation.hasRegister("%0"));
  EXPECT_TRUE(allocation.hasRegister("%1"));
  EXPECT_TRUE(allocation.hasRegister("%2"));
  EXPECT_TRUE(allocation.needsCallSaveSlot("%0"));
  EXPECT_TRUE(allocation.callSavedValues(2).find("%0") !=
              allocation.callSavedValues(2).end());
}

TEST_CASE(register_allocator_colors_scalar_parameters) {
  riscv::Function function;
  function.name = "add";
  function.params = {"@a", "@b"};
  function.param_types = {"i32", "i32"};
  function.instructions = {
      "%entry:",
      "  %0 = add @a, @b",
      "  ret %0",
  };

  riscv::RegisterAllocation allocation =
      riscv::RegisterAllocator().allocate(function);

  EXPECT_TRUE(allocation.hasRegister("@a"));
  EXPECT_TRUE(allocation.hasRegister("@b"));
  EXPECT_TRUE(allocation.registerFor("@a").rfind("t", 0) == 0);
  EXPECT_TRUE(allocation.registerFor("@b").rfind("t", 0) == 0);
}

TEST_CASE(register_allocator_uses_callee_saved_registers_under_pressure) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 0",
      "  %1 = add 2, 0",
      "  %2 = add 3, 0",
      "  %3 = add 4, 0",
      "  %4 = add 5, 0",
      "  %5 = add 6, 0",
      "  %6 = add 7, 0",
      "  %7 = add 8, 0",
      "  %8 = add 9, 0",
      "  %9 = add 10, 0",
      "  %10 = add 11, 0",
      "  %11 = add 12, 0",
      "  %12 = add %0, %1",
      "  %13 = add %12, %2",
      "  %14 = add %13, %3",
      "  %15 = add %14, %4",
      "  %16 = add %15, %5",
      "  %17 = add %16, %6",
      "  %18 = add %17, %7",
      "  %19 = add %18, %8",
      "  %20 = add %19, %9",
      "  %21 = add %20, %10",
      "  %22 = add %21, %11",
      "  ret %22",
  };

  riscv::RegisterAllocation allocation =
      riscv::RegisterAllocator().allocate(function);

  bool uses_callee_saved = false;
  for (const std::string &reg : allocation.usedRegisters()) {
    uses_callee_saved = uses_callee_saved || reg.rfind("s", 0) == 0;
  }
  EXPECT_TRUE(uses_callee_saved);
}

TEST_CASE(program_register_allocator_allocates_all_functions) {
  riscv::Program program;
  riscv::Function first;
  first.name = "first";
  first.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  ret %0",
  };
  riscv::Function second;
  second.name = "second";
  second.instructions = {
      "%entry:",
      "  %0 = add 3, 4",
      "  ret %0",
  };
  program.functions = {first, second};

  riscv::ProgramRegisterAllocation allocation =
      riscv::ProgramRegisterAllocator().allocate(program);

  EXPECT_TRUE(allocation.allocationFor("first").hasRegister("%0"));
  EXPECT_TRUE(allocation.allocationFor("second").hasRegister("%0"));
}
