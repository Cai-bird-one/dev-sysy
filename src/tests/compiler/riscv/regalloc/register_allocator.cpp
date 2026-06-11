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
