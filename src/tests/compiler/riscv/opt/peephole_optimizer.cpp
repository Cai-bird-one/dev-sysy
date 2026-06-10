#include "compiler/riscv/opt/peephole_optimizer.h"
#include "tests/test_framework.h"

#include <string>

using namespace compiler;

TEST_CASE(peephole_optimizer_removes_trivial_noops) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  li t0, 1\n"
                         "  mv t0, t0\n"
                         "  addi sp, sp, 0\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "  li t0, 1\n"
                       "  ret\n");
}

TEST_CASE(peephole_optimizer_forwards_adjacent_store_load) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  sw t0, 4(sp)\n"
                         "  lw a0, 4(sp)\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "  sw t0, 4(sp)\n"
                       "  mv a0, t0\n"
                       "  ret\n");
}

TEST_CASE(peephole_optimizer_replaces_power_of_two_scale_multiply) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  li t2, 4\n"
                         "  mul t1, t1, t2\n");

  EXPECT_EQ(optimized, "  li t2, 4\n"
                       "  slli t1, t1, 2\n");
}

TEST_CASE(peephole_optimizer_removes_jump_to_next_label) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  j main_end\n"
                         "main_end:\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "main_end:\n"
                       "  ret\n");
}
