#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_emits_minimal_function) {
  parser::ParseNode comp_unit("CompUnit");
  auto func_def = std::make_unique<parser::ParseNode>("FuncDef");
  func_def->children.push_back(std::make_unique<parser::ParseNode>("INT", "int"));
  func_def->children.push_back(
      std::make_unique<parser::ParseNode>("IDENT", "main"));
  func_def->children.push_back(
      std::make_unique<parser::ParseNode>("INT_CONST", "0"));
  comp_unit.children.push_back(std::move(func_def));

  ir::KoopaGenerator generator;
  std::string koopa = generator.generate(comp_unit);

  EXPECT_EQ(koopa, "fun @main(): i32 {\n%entry:\n  ret 0\n}\n");
}

TEST_CASE(koopa_generator_runs_after_lexer_and_parser) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main() { return 0; }");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_EQ(koopa, "fun @main(): i32 {\n%entry:\n  ret 0\n}\n");
}
