#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_emits_break_and_continue) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;while(a<5){a=a+1;if(a==2)continue;if(a==4)break;}return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("jump %while_entry_0") != std::string::npos);
  EXPECT_TRUE(koopa.find("jump %while_end_2") != std::string::npos);
  EXPECT_TRUE(koopa.find("%while_end_2:\n") != std::string::npos);
}

TEST_CASE(koopa_generator_hoists_loop_body_allocs_to_entry) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int i=0;while(i<3){int x=i;i=i+1;if(i==2)continue;}return i;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("%i = alloc i32\n  %x = alloc i32") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%while_body_1:\n  %3 = load %i\n  store %3, %x") !=
              std::string::npos);
}

TEST_CASE(koopa_generator_terminates_unreachable_loop_exit_block) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;while(1){a=a+1;if(a==3)return a;continue;}}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("%while_end_2:\n  ret 0\n") != std::string::npos);
}
