#include "compiler/riscv/frame/stack_frame.h"
#include "compiler/riscv/regalloc/register_allocator.h"
#include "tests/test_framework.h"

#include <map>
#include <string>
#include <vector>

using namespace compiler;

TEST_CASE(stack_frame_tracks_offsets_calls_and_pointer_dimensions) {
  riscv::Function function;
  function.name = "f";
  function.params = {"@a"};
  function.param_types = {"*[i32, 4]"};
  function.instructions = {
      "%entry:",
      "  %arr = alloc [i32, 2]",
      "  %p = getelemptr %arr, 1",
      "  %0 = call @g(@a)",
      "  ret %0",
  };

  std::map<std::string, std::vector<int>> globals;
  globals["glob"] = {3};

  riscv::StackFrame frame(function, globals,
                          riscv::RegisterAllocator().allocate(function));

  EXPECT_TRUE(frame.frameSize() >= 16);
  EXPECT_TRUE(frame.hasCall());
  EXPECT_TRUE(frame.raOffset() >= 0);
  EXPECT_TRUE(frame.hasRegisterValue("@a"));
  EXPECT_TRUE(!frame.hasStackValue("@a"));
  EXPECT_TRUE(frame.hasStackValue("%arr"));
  EXPECT_TRUE(frame.hasStackValue("%p"));
  EXPECT_TRUE(frame.isPointer("@a"));
  EXPECT_TRUE(frame.isPointer("%arr"));
  EXPECT_TRUE(frame.isAggregateAlloc("%arr"));
  EXPECT_EQ(frame.stackSizeOf("%arr"), 8);

  std::vector<int> param_dims = frame.dimensionsForPointer("@a");
  EXPECT_EQ(param_dims.size(), 1u);
  EXPECT_EQ(param_dims[0], 4);

  std::vector<int> global_dims = frame.dimensionsForPointer("@glob");
  EXPECT_EQ(global_dims.size(), 1u);
  EXPECT_EQ(global_dims[0], 3);
}

TEST_CASE(stack_frame_keeps_colored_temporaries_out_of_stack_slots) {
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

  EXPECT_TRUE(frame.hasRegisterValue("%0"));
  EXPECT_TRUE(frame.hasRegisterValue("%1"));
  EXPECT_TRUE(!frame.hasStackValue("%0"));
  EXPECT_TRUE(!frame.hasStackValue("%1"));
}
