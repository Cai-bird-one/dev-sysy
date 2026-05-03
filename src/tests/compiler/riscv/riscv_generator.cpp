#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "compiler/riscv/riscv_generator.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

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
