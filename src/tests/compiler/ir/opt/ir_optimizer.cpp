#include "compiler/ir/opt/ir_optimizer.h"
#include "compiler/ir/opt/analysis/control_flow_graph.h"
#include "compiler/ir/opt/analysis/liveness_analysis.h"
#include "compiler/riscv/riscv_generator.h"
#include "tests/test_framework.h"

#include <string>

using namespace compiler;

TEST_CASE(ir_liveness_tracks_values_across_basic_blocks) {
  ir::opt::IrFunction function;
  function.header = "fun @main(@a: i32): i32 {";
  function.instructions = {"%entry:", "%0 = add @a, 1", "jump %next",
                           "%next:", "ret %0"};

  ir::opt::ControlFlowGraph cfg = ir::opt::buildControlFlowGraph(function);
  ir::opt::LivenessInfo liveness = ir::opt::analyzeLiveness(function, cfg);

  EXPECT_EQ(cfg.blocks.size(), static_cast<size_t>(2));
  EXPECT_TRUE(liveness.live_out[1].find("%0") != liveness.live_out[1].end());
  EXPECT_TRUE(liveness.live_in[3].find("%0") != liveness.live_in[3].end());
  EXPECT_TRUE(liveness.live_in[4].find("%0") != liveness.live_in[4].end());
}

TEST_CASE(ir_optimizer_folds_constants_and_removes_dead_result) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = add 1, 2\n"
                         "  ret %0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret 3") != std::string::npos);
}

TEST_CASE(ir_optimizer_simplifies_algebraic_identity) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = load %a\n"
                         "  %1 = add %0, 0\n"
                         "  ret %1\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%1 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret %0") != std::string::npos);
}

TEST_CASE(ir_optimizer_removes_unreachable_code_after_terminator) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  ret 1\n"
                         "  %0 = add 2, 3\n"
                         "%next:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("%next:\n") == std::string::npos);
}

TEST_CASE(ir_optimizer_simplifies_constant_branches) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  br 0, %then, %else\n"
                         "%then:\n"
                         "  ret 1\n"
                         "%else:\n"
                         "  ret 2\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("br 0") == std::string::npos);
  EXPECT_TRUE(optimized.find("%then:\n") == std::string::npos);
  EXPECT_TRUE(optimized.find("%else:\n") != std::string::npos);
}

TEST_CASE(ir_optimizer_simplifies_same_target_branches) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@cond: i32): i32 {\n"
                         "%entry:\n"
                         "  br @cond, %exit, %exit\n"
                         "%exit:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("br @cond") == std::string::npos);
  EXPECT_TRUE(optimized.find("jump %exit") == std::string::npos);
  EXPECT_TRUE(optimized.find("%exit:\n") != std::string::npos);
}

TEST_CASE(ir_optimizer_propagates_constants_across_blocks) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = add 1, 2\n"
                         "  jump %next\n"
                         "%next:\n"
                         "  %1 = mul %0, 4\n"
                         "  ret %1\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("%1 = mul") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret 12") != std::string::npos);
}

TEST_CASE(ir_optimizer_propagates_copies_across_blocks) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@x: i32): i32 {\n"
                         "%entry:\n"
                         "  %0 = add @x, 0\n"
                         "  jump %next\n"
                         "%next:\n"
                         "  ret %0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret @x") != std::string::npos);
}

TEST_CASE(ir_optimizer_removes_unused_functions) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @used(): i32 {\n"
                         "%entry:\n"
                         "  ret 7\n"
                         "}\n"
                         "\n"
                         "fun @unused(): i32 {\n"
                         "%entry:\n"
                         "  ret 9\n"
                         "}\n"
                         "\n"
                         "fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = call @used()\n"
                         "  ret %0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("fun @used") != std::string::npos);
  EXPECT_TRUE(optimized.find("fun @unused") == std::string::npos);
  EXPECT_TRUE(optimized.find("call @used()") != std::string::npos);
}

TEST_CASE(ir_optimizer_threads_trivial_jumps) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  jump %mid\n"
                         "%mid:\n"
                         "  jump %exit\n"
                         "%exit:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("jump %mid") == std::string::npos);
  EXPECT_TRUE(optimized.find("jump %exit") == std::string::npos);
  EXPECT_TRUE(optimized.find("%exit:\n") != std::string::npos);
}

TEST_CASE(ir_optimizer_removes_jump_to_next_label) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  jump %next\n"
                         "%next:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("jump %next") == std::string::npos);
  EXPECT_TRUE(optimized.find("%next:\n") != std::string::npos);
  EXPECT_TRUE(optimized.find("ret 0") != std::string::npos);
}

TEST_CASE(ir_optimizer_removes_unreachable_blocks) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  jump %live\n"
                         "%dead:\n"
                         "  %0 = add 1, 2\n"
                         "  ret %0\n"
                         "%live:\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%dead:\n") == std::string::npos);
  EXPECT_TRUE(optimized.find("%0 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("%live:\n") != std::string::npos);
}

TEST_CASE(ir_optimizer_eliminates_basic_block_common_subexpressions) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@a: i32, @b: i32): i32 {\n"
                         "%entry:\n"
                         "  %0 = add @a, @b\n"
                         "  %1 = add @b, @a\n"
                         "  %2 = mul %1, 2\n"
                         "  ret %2\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%1 = add") == std::string::npos);
  EXPECT_TRUE(optimized.find("%2 = mul %0, 2") != std::string::npos);
}

TEST_CASE(ir_optimizer_forwards_local_scalar_loads) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@x: i32): i32 {\n"
                         "%entry:\n"
                         "  %a = alloc i32\n"
                         "  store @x, %a\n"
                         "  %0 = load %a\n"
                         "  %1 = add %0, 1\n"
                         "  ret %1\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%0 = load %a") == std::string::npos);
  EXPECT_TRUE(optimized.find("%1 = add @x, 1") != std::string::npos);
}

TEST_CASE(ir_optimizer_removes_dead_local_scalar_stores) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(@x: i32, @y: i32): i32 {\n"
                         "%entry:\n"
                         "  %a = alloc i32\n"
                         "  store @x, %a\n"
                         "  store @y, %a\n"
                         "  %0 = load %a\n"
                         "  ret %0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("store @x, %a") == std::string::npos);
  EXPECT_TRUE(optimized.find("store @y, %a") == std::string::npos);
  EXPECT_TRUE(optimized.find("ret @y") != std::string::npos);
}

TEST_CASE(ir_optimizer_keeps_local_stores_across_calls_and_jumps) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @touch(): i32 {\n"
                         "%entry:\n"
                         "  ret 0\n"
                         "}\n"
                         "\n"
                         "fun @main(@x: i32): i32 {\n"
                         "%entry:\n"
                         "  %a = alloc i32\n"
                         "  store @x, %a\n"
                         "  call @touch()\n"
                         "  jump %next\n"
                         "%next:\n"
                         "  %0 = load %a\n"
                         "  ret %0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("store @x, %a") != std::string::npos);
}

TEST_CASE(ir_optimizer_output_still_feeds_riscv_generator) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = mul 7, 6\n"
                         "  ret %0\n"
                         "}\n");

  riscv::RiscvGenerator generator;
  std::string assembly = generator.generate(optimized);
  EXPECT_TRUE(assembly.find("li a0, 42") != std::string::npos);
}

TEST_CASE(ir_optimizer_preserves_call_argument_values) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @id(@id_x: i32): i32 {\n"
                         "%entry:\n"
                         "  ret @id_x\n"
                         "}\n"
                         "\n"
                         "fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = load @g\n"
                         "  %1 = add %0, 1\n"
                         "  %2 = call @id(%1)\n"
                         "  ret %2\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("%1 = add %0, 1") != std::string::npos);
  EXPECT_TRUE(optimized.find("%2 = call @id(%1)") != std::string::npos);
}

TEST_CASE(ir_optimizer_keeps_call_commas_when_replacing_operands) {
  ir::opt::IrOptimizer optimizer;
  std::string optimized =
      optimizer.optimize("fun @sum(@a: i32, @b: i32): i32 {\n"
                         "%entry:\n"
                         "  ret @a\n"
                         "}\n"
                         "\n"
                         "fun @main(): i32 {\n"
                         "%entry:\n"
                         "  %0 = add 1, 2\n"
                         "  call @sum(%0, %0)\n"
                         "  ret 0\n"
                         "}\n");

  EXPECT_TRUE(optimized.find("call @sum(3, 3)") != std::string::npos);
}
