#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_resolves_nested_block_scopes) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=1; {int a=2;} return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  %a_1 = alloc i32\n"
            "  store 1, %a\n"
            "  store 2, %a_1\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_uses_outer_scope_in_shadowing_initializer) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream global_input("int a=1; int main(){int a=a+1; return a;}");
  std::unique_ptr<parser::ParseNode> ast =
      parser.parse(lexer.tokenize(global_input));

  EXPECT_EQ(generator.generate(*ast),
            "global @a = alloc i32, 1\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  %0 = load @a\n"
            "  %1 = add %0, 1\n"
            "  store %1, %a\n"
            "  %2 = load %a\n"
            "  ret %2\n"
            "}\n");

  std::istringstream local_input(
      "int main(){int a=1; {int a=a+1; return a;}}");
  ast = parser.parse(lexer.tokenize(local_input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  %a_1 = alloc i32\n"
            "  store 1, %a\n"
            "  %0 = load %a\n"
            "  %1 = add %0, 1\n"
            "  store %1, %a_1\n"
            "  %2 = load %a_1\n"
            "  ret %2\n"
            "}\n");
}
