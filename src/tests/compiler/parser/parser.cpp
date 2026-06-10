#include "compiler/parser/grammar_rules.h"
#include "compiler/lexer/token_rules.h"
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
  EXPECT_TRUE(tree->production_id >= 0);
  EXPECT_EQ(tree->children.size(), 2u);
  EXPECT_EQ(tree->children[0]->symbol, "TopItem");
  EXPECT_EQ(tree->children[1]->symbol, "CompUnitTail");
  EXPECT_TRUE(tree->children[0]->children[0]->children[0]->production_id >= 0);
  EXPECT_EQ(tree->children[0]
                ->children[0]
                ->children[0]
                ->children[0]
                ->production_id,
            -1);
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
  EXPECT_EQ(tree->children[1]->symbol, "CompUnitTail");
}

TEST_CASE(parser_parses_full_sysy_constructs) {
  Lexer lexer = buildDefaultLexer();
  Parser parser = buildDefaultParser();
  std::istringstream input(
      "const int n = 2;\n"
      "void sink(int a[], int b) { return; }\n"
      "int main() {\n"
      "  int x[2] = {1, 2};\n"
      "  if (x[0] < x[1] && n != 0) sink(x, n); else x[0] = 0;\n"
      "  while (x[0] <= 10 || !n) { break; continue; }\n"
      "  return x[0] + x[1] % 2;\n"
      "}\n");

  std::unique_ptr<ParseNode> tree = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(tree->symbol, "CompUnit");
}

TEST_CASE(parser_accepts_c_style_empty_void_params) {
  Lexer lexer = buildDefaultLexer();
  Parser parser = buildDefaultParser();
  std::istringstream input("int main(void) { return 0; }");

  std::unique_ptr<ParseNode> tree = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(tree->symbol, "CompUnit");
}

TEST_CASE(parser_accepts_trailing_commas_in_initializer_lists) {
  Lexer lexer = buildDefaultLexer();
  Parser parser = buildDefaultParser();
  std::istringstream input(
      "int main() { int a[2] = {1, 2,}; const int b[1] = {0,}; return 0; }");

  std::unique_ptr<ParseNode> tree = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(tree->symbol, "CompUnit");
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
