#include "compiler/riscv/opt/assembly_optimizer.h"
#include "compiler/riscv/opt/peephole_optimizer.h"
#include "tests/test_framework.h"

#include <algorithm>
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

TEST_CASE(peephole_optimizer_forwards_stack_loads_within_block) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  sw t0, 4(sp)\n"
                         "  li t2, 1\n"
                         "  lw a0, 4(sp)\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "  sw t0, 4(sp)\n"
                       "  li t2, 1\n"
                       "  mv a0, t0\n"
                       "  ret\n");
}

TEST_CASE(peephole_optimizer_does_not_forward_stack_loads_across_labels) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  sw t0, 4(sp)\n"
                         "next:\n"
                         "  lw a0, 4(sp)\n");

  EXPECT_EQ(optimized, "  sw t0, 4(sp)\n"
                       "next:\n"
                       "  lw a0, 4(sp)\n");
}

TEST_CASE(peephole_optimizer_does_not_forward_stack_loads_across_pointer_store) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  sw t0, 4(sp)\n"
                         "  sw t2, 0(t1)\n"
                         "  lw a0, 4(sp)\n");

  EXPECT_EQ(optimized, "  sw t0, 4(sp)\n"
                       "  sw t2, 0(t1)\n"
                       "  lw a0, 4(sp)\n");
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

TEST_CASE(peephole_optimizer_inverts_small_branch_to_next_label) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  bnez t0, loop_body\n"
                         "  j loop_end\n"
                         "loop_body:\n"
                         "  ret\n"
                         "loop_end:\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "  beqz t0, loop_end\n"
                       "loop_body:\n"
                       "  ret\n"
                       "loop_end:\n"
                       "  ret\n");
}

TEST_CASE(peephole_optimizer_removes_redundant_adjacent_moves_in_small_code) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  mv t5, t0\n"
                         "  mv t0, t5\n"
                         "  mv a0, t1\n"
                         "  mv a0, t1\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "  mv t5, t0\n"
                       "  mv a0, t1\n"
                       "  ret\n");
}

TEST_CASE(peephole_optimizer_aggressively_rewrites_large_branchy_code) {
  riscv::AssemblyOptimizer optimizer;
  std::string assembly = "  .text\n"
                         "  .globl hot\n"
                         "hot:\n";
  for (int i = 0; i < 1200; ++i) {
    assembly += "  li t0, 0\n";
  }
  for (int i = 0; i < 20; ++i) {
    std::string suffix = std::to_string(i);
    assembly += "  bnez t0, hot_body_" + suffix + "\n";
    assembly += "  j hot_end_" + suffix + "\n";
    assembly += "hot_body_" + suffix + ":\n";
    assembly += "  mv t5, t0\n";
    assembly += "  mv t0, t5\n";
    assembly += "hot_end_" + suffix + ":\n";
  }

  std::string optimized = optimizer.optimize(assembly);
  EXPECT_TRUE(optimized.find("  beqz t0, hot_end_0\n"
                             "hot_body_0:\n") != std::string::npos);
  EXPECT_TRUE(optimized.find("  bnez t0, hot_body_0\n"
                             "  j hot_end_0\n"
                             "hot_body_0:\n") == std::string::npos);
  EXPECT_TRUE(optimized.find("  mv t5, t0\n"
                             "  mv t0, t5\n") == std::string::npos);
  EXPECT_TRUE(std::count(optimized.begin(), optimized.end(), '\n') <
              std::count(assembly.begin(), assembly.end(), '\n'));
}

TEST_CASE(peephole_optimizer_keeps_independent_immediates_and_addresses) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  li t0, 42\n"
                         "  li t1, 42\n"
                         "  la t0, glob\n"
                         "  la a0, glob\n");

  EXPECT_EQ(optimized, "  li t0, 42\n"
                       "  li t1, 42\n"
                       "  la t0, glob\n"
                       "  la a0, glob\n");
}

TEST_CASE(peephole_optimizer_simplifies_zero_arithmetic) {
  riscv::PeepholeOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  add t0, t1, zero\n"
                         "  sub t2, t0, zero\n"
                         "  mul a0, zero, t2\n"
                         "  addi a1, a0, 0\n");

  EXPECT_EQ(optimized, "  mv t0, t1\n"
                       "  mv t2, t0\n"
                       "  li a0, 0\n"
                       "  mv a1, a0\n");
}

TEST_CASE(assembly_optimizer_iterates_riscv_passes_to_fixed_point) {
  riscv::AssemblyOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("  j done\n"
                         "  j done\n"
                         "done:\n"
                         "  ret\n");

  EXPECT_EQ(optimized, "done:\n"
                       "  ret\n");
  EXPECT_EQ(optimizer.optimize(optimized), optimized);
}
