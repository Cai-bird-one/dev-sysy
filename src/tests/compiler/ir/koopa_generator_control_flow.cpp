#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_booleanizes_runtime_logical_operands) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=2,b=4; return a&&b;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  std::string koopa = generator.generate(*ast);
  EXPECT_TRUE(koopa.find("br %1, %and_rhs_0, %and_end_1") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%and_rhs_0:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("%and_end_1:\n") != std::string::npos);

  std::istringstream or_input("int main(){int a=2,b=4; return a||b;}");
  ast = parser.parse(lexer.tokenize(or_input));

  koopa = generator.generate(*ast);
  EXPECT_TRUE(koopa.find("br %1, %or_end_1, %or_rhs_0") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%or_rhs_0:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("%or_end_1:\n") != std::string::npos);
}

TEST_CASE(koopa_generator_emits_if_else_branches) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=0;if(1)a=1;else a=2;return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  br 1, %if_then_0, %if_else_1\n"
            "%if_then_0:\n"
            "  store 1, %a\n"
            "  jump %if_end_2\n"
            "%if_else_1:\n"
            "  store 2, %a\n"
            "  jump %if_end_2\n"
            "%if_end_2:\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_matches_else_to_nearest_if) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;if(1)if(0)a=1;else a=2;return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  br 1, %if_then_0, %if_end_2\n"
            "%if_then_0:\n"
            "  br 0, %if_then_3, %if_else_4\n"
            "%if_then_3:\n"
            "  store 1, %a\n"
            "  jump %if_end_5\n"
            "%if_else_4:\n"
            "  store 2, %a\n"
            "  jump %if_end_5\n"
            "%if_end_5:\n"
            "  jump %if_end_2\n"
            "%if_end_2:\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_while_loop) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;while(a<3)a=a+1;return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  std::string koopa = generator.generate(*ast);
  EXPECT_TRUE(koopa.find("%while_entry_0:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("%while_body_1:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("%while_end_2:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("br %2, %while_body_1, %while_end_2") !=
              std::string::npos);
}
