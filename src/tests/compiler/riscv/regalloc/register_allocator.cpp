#include "compiler/riscv/regalloc/register_allocator.h"
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

TEST_CASE(register_allocator_skips_functions_with_calls) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %0 = add 1, 2",
      "  %1 = call @f(%0)",
      "  ret %1",
  };

  riscv::RegisterAllocation allocation =
      riscv::RegisterAllocator().allocate(function);

  EXPECT_TRUE(!allocation.hasRegister("%0"));
  EXPECT_TRUE(!allocation.hasRegister("%1"));
}
