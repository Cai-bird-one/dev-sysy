#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_rejects_duplicate_names_in_same_scope) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream local_input("int main(){int a; const int a=1; return a;}");
  std::unique_ptr<parser::ParseNode> ast =
      parser.parse(lexer.tokenize(local_input));

  bool rejected_local = false;
  try {
    generator.generate(*ast);
  } catch (const ir::IrError &) {
    rejected_local = true;
  }
  EXPECT_TRUE(rejected_local);

  std::istringstream global_input("int a; const int a=1; int main(){return 0;}");
  ast = parser.parse(lexer.tokenize(global_input));

  bool rejected_global = false;
  try {
    generator.generate(*ast);
  } catch (const ir::IrError &) {
    rejected_global = true;
  }
  EXPECT_TRUE(rejected_global);
}
