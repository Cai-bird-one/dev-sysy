#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/sdt/production_rules.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

namespace {

std::unique_ptr<parser::ParseNode>
node(const std::string &symbol, std::initializer_list<std::string> rhs) {
  auto result = std::make_unique<parser::ParseNode>(symbol);
  result->production_id = ir::sdt::findProductionId(symbol, rhs);
  return result;
}

std::unique_ptr<parser::ParseNode> terminal(const std::string &symbol,
                                            const std::string &lexeme = "") {
  return std::make_unique<parser::ParseNode>(symbol, lexeme);
}

std::unique_ptr<parser::ParseNode> zeroExpression() {
  auto number = node("Number", {"INT_CONST"});
  number->children.push_back(terminal("INT_CONST", "0"));

  auto primary_exp = node("PrimaryExp", {"Number"});
  primary_exp->children.push_back(std::move(number));

  auto unary_exp = node("UnaryExp", {"PrimaryExp"});
  unary_exp->children.push_back(std::move(primary_exp));

  auto mul_tail = node("MulExpTail", {});
  auto mul_exp = node("MulExp", {"UnaryExp", "MulExpTail"});
  mul_exp->children.push_back(std::move(unary_exp));
  mul_exp->children.push_back(std::move(mul_tail));

  auto add_tail = node("AddExpTail", {});
  auto add_exp = node("AddExp", {"MulExp", "AddExpTail"});
  add_exp->children.push_back(std::move(mul_exp));
  add_exp->children.push_back(std::move(add_tail));

  auto rel_tail = node("RelExpTail", {});
  auto rel_exp = node("RelExp", {"AddExp", "RelExpTail"});
  rel_exp->children.push_back(std::move(add_exp));
  rel_exp->children.push_back(std::move(rel_tail));

  auto eq_tail = node("EqExpTail", {});
  auto eq_exp = node("EqExp", {"RelExp", "EqExpTail"});
  eq_exp->children.push_back(std::move(rel_exp));
  eq_exp->children.push_back(std::move(eq_tail));

  auto land_tail = node("LAndExpTail", {});
  auto land_exp = node("LAndExp", {"EqExp", "LAndExpTail"});
  land_exp->children.push_back(std::move(eq_exp));
  land_exp->children.push_back(std::move(land_tail));

  auto lor_tail = node("LOrExpTail", {});
  auto lor_exp = node("LOrExp", {"LAndExp", "LOrExpTail"});
  lor_exp->children.push_back(std::move(land_exp));
  lor_exp->children.push_back(std::move(lor_tail));

  auto exp = node("Exp", {"LOrExp"});
  exp->children.push_back(std::move(lor_exp));
  return exp;
}

} // namespace

TEST_CASE(koopa_generator_emits_minimal_function) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  std::istringstream input("int main() { return 0; }");
  std::unique_ptr<parser::ParseNode> comp_unit =
      parser.parse(lexer.tokenize(input));

  ir::KoopaGenerator generator;
  std::string koopa = generator.generate(*comp_unit);

  EXPECT_EQ(koopa, "fun @main(): i32 {\n%entry:\n  ret 0\n}\n");
}

TEST_CASE(koopa_generator_emits_from_manual_ast) {
  parser::ParseNode comp_unit("CompUnit");
  auto return_exp = node("ReturnExpOpt", {"Exp"});
  return_exp->children.push_back(zeroExpression());

  auto stmt = node("Stmt", {"RETURN", "ReturnExpOpt", "SEMICOLON"});
  stmt->children.push_back(terminal("RETURN", "return"));
  stmt->children.push_back(std::move(return_exp));
  stmt->children.push_back(terminal("SEMICOLON", ";"));
  auto block = std::make_unique<parser::ParseNode>("Block");
  block->children.push_back(std::move(stmt));
  auto func_def = std::make_unique<parser::ParseNode>("FuncDef");
  func_def->children.push_back(
      std::make_unique<parser::ParseNode>("IDENT", "main"));
  func_def->children.push_back(std::move(block));
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
  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n  ret 42\n}\n");

  std::istringstream octal_input("int main(){return 052;}");
  ast = parser.parse(lexer.tokenize(octal_input));
  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n  ret 42\n}\n");
}

TEST_CASE(koopa_generator_handles_unary_constant_expressions) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){return -+!0;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n  ret -1\n}\n");

  std::istringstream paren_input("int main(){return (1);}");
  ast = parser.parse(lexer.tokenize(paren_input));
  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n  ret 1\n}\n");
}

TEST_CASE(koopa_generator_handles_binary_constant_expressions) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){const int a=1+2*3,b=(a-1)%4;return b;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n  ret 2\n}\n");
}
