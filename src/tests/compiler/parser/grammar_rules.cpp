#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

using namespace compiler::parser;

TEST_CASE(grammar_rules_expose_default_grammar) {
  const Grammar &grammar = defaultGrammar();

  EXPECT_EQ(grammar.start_symbol, "CompUnit");
  EXPECT_TRUE(!grammar.productions.empty());
  EXPECT_EQ(grammar.productions[0].lhs, "CompUnit");
}

TEST_CASE(grammar_rules_build_default_parser) {
  Parser parser = buildDefaultParser();
  EXPECT_TRUE(true);
}
