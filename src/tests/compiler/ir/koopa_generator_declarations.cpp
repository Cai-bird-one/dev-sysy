#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_handles_global_declarations) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("const int a = 10, b = 5; int main(){return b;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n  ret 5\n}\n");

  std::istringstream var_input("int g = 7, h; int main(){return g;}");
  ast = parser.parse(lexer.tokenize(var_input));
  EXPECT_EQ(generator.generate(*ast),
            "global @g = alloc i32, 7\n"
            "global @h = alloc i32, zeroinit\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  %0 = load @g\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_global_variable_store) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int g; int main(){g=3; return g;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "global @g = alloc i32, zeroinit\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  store 3, @g\n"
            "  %0 = load @g\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_local_variable_storage) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=1; a=a+2; return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 1, %a\n"
            "  %0 = load %a\n"
            "  %1 = add %0, 2\n"
            "  store %1, %a\n"
            "  %2 = load %a\n"
            "  ret %2\n"
            "}\n");
}

TEST_CASE(koopa_generator_initializes_unassigned_local_variables) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a; return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_allows_local_scope_to_shadow_globals) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int a=1; int main(){int a=2; return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "global @a = alloc i32, 1\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 2, %a\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}
