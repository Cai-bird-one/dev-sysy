#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "compiler/riscv/riscv_generator.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>
#include <string>

using namespace compiler;

TEST_CASE(riscv_generator_emits_return_function) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  ret 42\n"
                                         "}\n");

  EXPECT_EQ(riscv, "  .text\n"
                   "  .globl main\n"
                   "main:\n"
                   "  li a0, 42\n"
                   "  ret\n");
}

TEST_CASE(riscv_generator_handles_negative_return_values) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  ret -1\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("li a0, -1") != std::string::npos);
}

TEST_CASE(riscv_generator_runs_after_koopa_generation) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator koopa_generator;
  riscv::RiscvGenerator riscv_generator;

  std::istringstream input("int main(){return 1+2*3;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = koopa_generator.generate(*ast);
  std::string riscv = riscv_generator.generate(koopa);

  EXPECT_TRUE(riscv.find("main:") != std::string::npos);
  EXPECT_TRUE(riscv.find("li a0, 7") != std::string::npos);
  EXPECT_TRUE(riscv.find("ret") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_stack_variables) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  %a = alloc i32\n"
                                         "  store 1, %a\n"
                                         "  %0 = load %a\n"
                                         "  %1 = add %0, 2\n"
                                         "  store %1, %a\n"
                                         "  %2 = load %a\n"
                                         "  ret %2\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("li t0, -16") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw t0, 0(sp)") != std::string::npos);
  EXPECT_TRUE(riscv.find("add t0, t0, t1") != std::string::npos);
  EXPECT_TRUE(riscv.find("mv a0, ") != std::string::npos);
  EXPECT_TRUE(riscv.find("ret") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_global_variables) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("global @g = alloc i32, 7\n"
                                         "\n"
                                         "fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  %0 = load @g\n"
                                         "  ret %0\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("  .data\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("g:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("  .word 7\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("la t0, g") != std::string::npos);
  EXPECT_TRUE(riscv.find("lw t0, 0(t0)") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_arrays_and_getelemptr) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate(
      "global @a = alloc [[i32, 3], 2], {{1, 2, 0}, {3, 0, 0}}\n"
      "\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  %b = alloc [i32, 2]\n"
      "  %0 = getelemptr @a, 1\n"
      "  %1 = getelemptr %0, 2\n"
      "  store 4, %1\n"
      "  %2 = getelemptr %b, 0\n"
      "  store 5, %2\n"
      "  %3 = load %1\n"
      "  ret %3\n"
      "}\n");

  EXPECT_TRUE(riscv.find("a:\n  .word 1\n  .word 2\n  .word 0\n  .word 3") !=
              std::string::npos);
  EXPECT_TRUE(riscv.find("li t2, 12") != std::string::npos);
  EXPECT_TRUE(riscv.find("li t2, 4") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw t0, 0(t1)") != std::string::npos);
}

TEST_CASE(riscv_generator_treats_single_element_arrays_as_stack_addresses) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  %a = alloc [i32, 1]\n"
                                         "  %0 = getelemptr %a, 0\n"
                                         "  store 7, %0\n"
                                         "  %1 = load %0\n"
                                         "  ret %1\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("addi t0, sp, ") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_array_parameters_and_getptr) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate(
      "fun @sum(@a: *i32, @b: *[i32, 3]): i32 {\n"
      "%entry:\n"
      "  %0 = getptr @a, 1\n"
      "  %1 = load %0\n"
      "  %2 = getptr @b, 1\n"
      "  %3 = getelemptr %2, 2\n"
      "  %4 = load %3\n"
      "  %5 = add %1, %4\n"
      "  ret %5\n"
      "}\n");

  EXPECT_TRUE(riscv.find("sum:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw a0") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw a1") != std::string::npos);
  EXPECT_TRUE(riscv.find("li t2, 12") != std::string::npos);
  EXPECT_TRUE(riscv.find("li t2, 4") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_comparisons) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  %0 = lt 1, 2\n"
                                         "  %1 = eq %0, 1\n"
                                         "  ret %1\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("slt t0, t0, t1") != std::string::npos);
  EXPECT_TRUE(riscv.find("seqz t0, t0") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_branches_and_jumps) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  br 1, %then, %else\n"
                                         "%then:\n"
                                         "  ret 1\n"
                                         "%else:\n"
                                         "  jump %then\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("bnez t0, main_then") != std::string::npos);
  EXPECT_TRUE(riscv.find("j main_else") != std::string::npos);
  EXPECT_TRUE(riscv.find("main_then:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("j main_then") != std::string::npos);
}

TEST_CASE(riscv_generator_keeps_control_flow_labels_function_local) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate(
      "fun @f(): i32 {\n"
      "%entry:\n"
      "  br 1, %then, %end\n"
      "%then:\n"
      "  jump %end\n"
      "%end:\n"
      "  ret 0\n"
      "}\n"
      "\n"
      "fun @g(): i32 {\n"
      "%entry:\n"
      "  br 1, %then, %end\n"
      "%then:\n"
      "  jump %end\n"
      "%end:\n"
      "  ret 1\n"
      "}\n");

  EXPECT_TRUE(riscv.find("bnez t0, f_then") != std::string::npos);
  EXPECT_TRUE(riscv.find("j f_end") != std::string::npos);
  EXPECT_TRUE(riscv.find("f_then:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("g_then:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("\nthen:\n") == std::string::npos);
}

TEST_CASE(riscv_generator_handles_large_stack_offsets) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate("fun @main(): i32 {\n"
                                         "%entry:\n"
                                         "  %big = alloc [i32, 600]\n"
                                         "  %x = alloc i32\n"
                                         "  store 7, %x\n"
                                         "  %0 = load %x\n"
                                         "  ret %0\n"
                                         "}\n");

  EXPECT_TRUE(riscv.find("li t2, 2400") != std::string::npos);
  EXPECT_TRUE(riscv.find("add t2, sp, t2") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw t0, 0(t2)") != std::string::npos);
  EXPECT_TRUE(riscv.find("lw t0, 0(t2)") != std::string::npos);
}

TEST_CASE(riscv_generator_handles_functions_and_calls) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate(
      "fun @add(@a: i32, @b: i32): i32 {\n"
      "%entry:\n"
      "  %a = alloc i32\n"
      "  %b = alloc i32\n"
      "  store @a, %a\n"
      "  store @b, %b\n"
      "  %0 = load %a\n"
      "  %1 = load %b\n"
      "  %2 = add %0, %1\n"
      "  ret %2\n"
      "}\n"
      "\n"
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  %0 = call @add(1, 2)\n"
      "  ret %0\n"
      "}\n");

  EXPECT_TRUE(riscv.find("add:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw a0") != std::string::npos);
  EXPECT_TRUE(riscv.find("sw a1") != std::string::npos);
  EXPECT_TRUE(riscv.find("main:\n") != std::string::npos);
  EXPECT_TRUE(riscv.find("call add") != std::string::npos);
  EXPECT_TRUE(riscv.find("lw ra") != std::string::npos);
}

TEST_CASE(riscv_generator_saves_register_values_live_across_calls) {
  riscv::RiscvGenerator generator;
  std::string riscv = generator.generate(
      "fun @main(): i32 {\n"
      "%entry:\n"
      "  %0 = add 1, 2\n"
      "  %1 = call @f()\n"
      "  %2 = add %0, %1\n"
      "  ret %2\n"
      "}\n");

  size_t save = riscv.find("  sw ");
  size_t call = riscv.find("  call f\n");
  size_t restore = riscv.find("  call f\n  lw ");

  EXPECT_TRUE(save != std::string::npos);
  EXPECT_TRUE(call != std::string::npos);
  EXPECT_TRUE(restore != std::string::npos);
  EXPECT_TRUE(save < call);
}

TEST_CASE(riscv_generator_optimized_output_uses_peephole_pass) {
  riscv::RiscvGenerator generator;
  std::string normal = generator.generate("fun @main(): i32 {\n"
                                          "%entry:\n"
                                          "  %a = alloc i32\n"
                                          "  %0 = call @id()\n"
                                          "  store %0, %a\n"
                                          "  %1 = load %a\n"
                                          "  ret %1\n"
                                          "}\n");
  std::string optimized = generator.generateOptimized("fun @main(): i32 {\n"
                                                      "%entry:\n"
                                                      "  %a = alloc i32\n"
                                                      "  %0 = call @id()\n"
                                                      "  store %0, %a\n"
                                                      "  %1 = load %a\n"
                                                      "  ret %1\n"
                                                      "}\n");

  EXPECT_TRUE(normal.find("  call id\n") != std::string::npos);
  EXPECT_TRUE(optimized.size() < normal.size());
}
