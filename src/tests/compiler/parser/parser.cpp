#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <sstream>

using namespace compiler::lexer;
using namespace compiler::parser;

TEST_CASE(parser_parses_minimal_function) {
  std::vector<Token> tokens = {
      {"INT", "int"},       {"IDENT", "main"},    {"LPAREN", "("},
      {"RPAREN", ")"},      {"LBRACE", "{"},      {"RETURN", "return"},
      {"INT_CONST", "0"},   {"SEMICOLON", ";"},   {"RBRACE", "}"},
  };

  Parser parser = buildDefaultParser();
  std::unique_ptr<ParseNode> tree = parser.parse(tokens);

  EXPECT_EQ(tree->symbol, "CompUnit");
  EXPECT_EQ(tree->children.size(), 2u);
  EXPECT_EQ(tree->children[0]->symbol, "TopItem");
  EXPECT_EQ(tree->children[1]->symbol, "TopItems");
}

TEST_CASE(parser_parses_global_const_before_function) {
  std::vector<Token> tokens = {
      {"CONST", "const"},   {"INT", "int"},       {"IDENT", "a"},
      {"ASSIGN", "="},      {"INT_CONST", "10"},  {"COMMA", ","},
      {"IDENT", "b"},       {"ASSIGN", "="},      {"INT_CONST", "5"},
      {"SEMICOLON", ";"},   {"INT", "int"},       {"IDENT", "main"},
      {"LPAREN", "("},      {"RPAREN", ")"},      {"LBRACE", "{"},
      {"RETURN", "return"}, {"IDENT", "b"},       {"SEMICOLON", ";"},
      {"RBRACE", "}"},
  };

  Parser parser = buildDefaultParser();
  std::unique_ptr<ParseNode> tree = parser.parse(tokens);

  EXPECT_EQ(tree->symbol, "CompUnit");
  EXPECT_EQ(tree->children[0]->symbol, "TopItem");
  EXPECT_EQ(tree->children[1]->symbol, "TopItems");
}

TEST_CASE(parser_rejects_unexpected_token) {
  std::vector<Token> tokens = {
      {"RETURN", "return"},
      {"INT_CONST", "0"},
      {"SEMICOLON", ";"},
  };

  Parser parser = buildDefaultParser();
  bool thrown = false;
  try {
    parser.parse(tokens);
  } catch (const ParserError &) {
    thrown = true;
  }

  EXPECT_TRUE(thrown);
}

TEST_CASE(parser_can_print_parse_tree) {
  std::vector<Token> tokens = {
      {"INT", "int"},       {"IDENT", "main"},    {"LPAREN", "("},
      {"RPAREN", ")"},      {"LBRACE", "{"},      {"RETURN", "return"},
      {"INT_CONST", "0"},   {"SEMICOLON", ";"},   {"RBRACE", "}"},
  };

  Parser parser = buildDefaultParser();
  std::unique_ptr<ParseNode> tree = parser.parse(tokens);
  std::ostringstream output;
  printParseTree(*tree, output);

  EXPECT_TRUE(output.str().find("CompUnit") != std::string::npos);
  EXPECT_TRUE(output.str().find("RETURN \"return\"") != std::string::npos);
}

TEST_CASE(parser_rejects_terminal_without_token_case_name) {
  Grammar grammar = {
      "Start",
      {
          {"Start", {"badTerminal"}},
      },
  };

  bool thrown = false;
  try {
    ParserBuilder(grammar).setAvailableTokens({"badTerminal"}).build();
  } catch (const ParserError &) {
    thrown = true;
  }

  EXPECT_TRUE(thrown);
}

TEST_CASE(parser_rejects_terminal_without_matching_token) {
  Grammar grammar = {
      "Start",
      {
          {"Start", {"MISSING_TOKEN"}},
      },
  };

  bool thrown = false;
  try {
    ParserBuilder(grammar).setAvailableTokens({"OTHER_TOKEN"}).build();
  } catch (const ParserError &) {
    thrown = true;
  }

  EXPECT_TRUE(thrown);
}

TEST_CASE(parser_rejects_nonterminal_with_token_case_name) {
  Grammar grammar = {
      "START",
      {
          {"START", {"INT"}},
      },
  };

  bool thrown = false;
  try {
    ParserBuilder(grammar).setAvailableTokens({"INT"}).build();
  } catch (const ParserError &) {
    thrown = true;
  }

  EXPECT_TRUE(thrown);
}

TEST_CASE(parser_rejects_unknown_rhs_symbol) {
  Grammar grammar = {
      "Start",
      {
          {"Start", {"INT", "Unknown"}},
      },
  };

  bool thrown = false;
  try {
    ParserBuilder(grammar).setAvailableTokens({"INT"}).build();
  } catch (const ParserError &) {
    thrown = true;
  }

  EXPECT_TRUE(thrown);
}
