#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/emit/operand_emitter.h"
#include "compiler/riscv/frame/stack_frame.h"
#include "compiler/riscv/regalloc/register_allocator.h"
#include "tests/test_framework.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace compiler;

TEST_CASE(operand_emitter_loads_values_and_stores_through_pointers) {
  riscv::Function function;
  function.name = "main";
  function.instructions = {
      "%entry:",
      "  %x = alloc i32",
      "  %arr = alloc [i32, 2]",
      "  %p = getelemptr %arr, 1",
      "  ret 0",
  };

  riscv::StackFrame frame(function, {},
                          riscv::RegisterAllocator().allocate(function));
  std::ostringstream output;
  riscv::AssemblyEmitter asm_output(output);
  riscv::OperandEmitter operands(frame, asm_output);

  operands.loadOperand("7", "t0");
  operands.loadOperand("%x", "t1");
  operands.emitPointerAddress("%arr", "t2");
  operands.storeToPointer("t0", "%p");
  operands.storeValue("t0", "%x");

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  li t0, 7\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  lw t1, ") != std::string::npos);
  EXPECT_TRUE(riscv.find("  addi t2, sp, ") != std::string::npos);
  EXPECT_TRUE(riscv.find("  sw t0, 0(t1)\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  sw t0, ") != std::string::npos);
}
