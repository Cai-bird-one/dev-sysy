#include "compiler/lexer/token_rules.h"
#include "tests/test_framework.h"

#include <sstream>

using namespace compiler::lexer;

TEST_CASE(token_rules_build_default_lexer) {
  Lexer lexer = buildDefaultLexer();
  EXPECT_TRUE(!lexer.hasAmbiguity());
}

TEST_CASE(token_rules_tokenize_basic_expression) {
  Lexer lexer = buildDefaultLexer();
  std::istringstream input("value + 100;");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].name, "IDENT");
  EXPECT_EQ(tokens[0].lexeme, "value");
  EXPECT_EQ(tokens[1].name, "PLUS");
  EXPECT_EQ(tokens[1].lexeme, "+");
  EXPECT_EQ(tokens[2].name, "INT_CONST");
  EXPECT_EQ(tokens[2].lexeme, "100");
  EXPECT_EQ(tokens[3].name, "SEMICOLON");
  EXPECT_EQ(tokens[3].lexeme, ";");
}

TEST_CASE(token_rules_tokenize_flex_style_sample) {
  Lexer lexer = buildDefaultLexer();
  std::istringstream input("int main() {\n"
                           "  // comment should be skipped\n"
                           "  return 0;\n"
                           "}");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 9u);
  EXPECT_EQ(tokens[0].name, "INT");
  EXPECT_EQ(tokens[0].lexeme, "int");
  EXPECT_EQ(tokens[1].name, "IDENT");
  EXPECT_EQ(tokens[1].lexeme, "main");
  EXPECT_EQ(tokens[2].name, "LPAREN");
  EXPECT_EQ(tokens[3].name, "RPAREN");
  EXPECT_EQ(tokens[4].name, "LBRACE");
  EXPECT_EQ(tokens[5].name, "RETURN");
  EXPECT_EQ(tokens[5].lexeme, "return");
  EXPECT_EQ(tokens[6].name, "INT_CONST");
  EXPECT_EQ(tokens[6].lexeme, "0");
  EXPECT_EQ(tokens[7].name, "SEMICOLON");
  EXPECT_EQ(tokens[8].name, "RBRACE");
}

TEST_CASE(token_rules_tokenize_integer_literal_forms) {
  Lexer lexer = buildDefaultLexer();
  std::istringstream input("123 077 0x2a 0XCAFE");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].name, "INT_CONST");
  EXPECT_EQ(tokens[0].lexeme, "123");
  EXPECT_EQ(tokens[1].name, "INT_CONST");
  EXPECT_EQ(tokens[1].lexeme, "077");
  EXPECT_EQ(tokens[2].name, "INT_CONST");
  EXPECT_EQ(tokens[2].lexeme, "0x2a");
  EXPECT_EQ(tokens[3].name, "INT_CONST");
  EXPECT_EQ(tokens[3].lexeme, "0XCAFE");
}

TEST_CASE(token_rules_fallback_matches_single_character) {
  Lexer lexer = buildDefaultLexer();
  std::istringstream input("@");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].name, "CHAR");
  EXPECT_EQ(tokens[0].lexeme, "@");
}

TEST_CASE(token_rules_expose_rule_table) {
  const std::vector<RegexTokenRule> &rules = defaultRegexTokenRules();

  EXPECT_TRUE(!rules.empty());
  EXPECT_EQ(std::string(rules[0].name), "WHITESPACE");
  EXPECT_EQ(static_cast<int>(rules[0].action),
            static_cast<int>(TokenRuleAction::Skip));
}
