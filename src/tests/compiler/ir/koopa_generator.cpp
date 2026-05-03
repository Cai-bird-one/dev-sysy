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
  auto exp = std::make_unique<parser::ParseNode>("Exp");
  auto unary_exp = std::make_unique<parser::ParseNode>("UnaryExp");
  auto primary_exp = std::make_unique<parser::ParseNode>("PrimaryExp");
  auto number = std::make_unique<parser::ParseNode>("Number");
  number->children.push_back(std::make_unique<parser::ParseNode>("INT_CONST", "0"));
  primary_exp->children.push_back(std::move(number));
  unary_exp->children.push_back(std::move(primary_exp));
  exp->children.push_back(std::move(unary_exp));
  func_def->children.push_back(std::move(exp));
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

TEST_CASE(koopa_generator_normalizes_integer_literals) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){return 0x2a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 42\n}\n");

  std::istringstream octal_input("int main(){return 052;}");
  ast = parser.parse(lexer.tokenize(octal_input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 42\n}\n");
}

TEST_CASE(koopa_generator_handles_unary_constant_expressions) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){return -+!0;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret -1\n}\n");

  std::istringstream paren_input("int main(){return (1);}");
  ast = parser.parse(lexer.tokenize(paren_input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 1\n}\n");
}
