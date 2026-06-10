#include "compiler/riscv/emit/assembly_emitter.h"
#include "tests/test_framework.h"

#include <sstream>

using namespace compiler;

TEST_CASE(assembly_emitter_formats_sections_labels_and_large_stack_offsets) {
  std::ostringstream output;
  riscv::AssemblyEmitter emitter(output);

  emitter.sectionData();
  emitter.global("g");
  emitter.label("g");
  emitter.word(42);
  emitter.sectionText();
  emitter.loadImmediate("t0", -16);
  emitter.loadAddress("t1", "g");
  emitter.loadWord("a0", 3000);
  emitter.storeWord("a0", 3004);
  emitter.loadStackAddress(3008, "t0");
  emitter.adjustStack(-32);

  std::string riscv = output.str();
  EXPECT_TRUE(riscv.find("  .data\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  .globl g\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("g:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  .word 42\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  li t2, 3000\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  lw a0, 0(t2)\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  sw a0, 0(t2)\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  add t0, sp, t0\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  add sp, sp, t0\n") != std::string::npos);
}
