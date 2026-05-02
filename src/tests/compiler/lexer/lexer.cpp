#include "compiler/lexer/lexer.h"
#include "tests/test_framework.h"

#include <sstream>

using namespace compiler::lexer;

TEST_CASE(lexer_tokenizes_regex_rules_with_longest_match) {
  Lexer lexer = LexerBuilder()
                    .addTokenRegex("IDENT", "[a-zA-Z_][a-zA-Z0-9_]*")
                    .addTokenRegex("PLUS", "[+]")
                    .addTokenRegex("INT", "[0-9]+")
                    .build();

  std::istringstream input("abc123+42");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens[0].name, "IDENT");
  EXPECT_EQ(tokens[0].lexeme, "abc123");
  EXPECT_EQ(tokens[1].name, "PLUS");
  EXPECT_EQ(tokens[1].lexeme, "+");
  EXPECT_EQ(tokens[2].name, "INT");
  EXPECT_EQ(tokens[2].lexeme, "42");
}

TEST_CASE(lexer_skips_rules_marked_as_skip) {
  Lexer lexer = LexerBuilder()
                    .skipRegex("WHITESPACE", "[ \\t\\n\\r]+")
                    .addTokenRegex("IDENT", "[a-zA-Z_][a-zA-Z0-9_]*")
                    .addTokenRegex("INT", "[0-9]+")
                    .build();

  std::istringstream input("sum \n 123");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0].name, "IDENT");
  EXPECT_EQ(tokens[0].lexeme, "sum");
  EXPECT_EQ(tokens[1].name, "INT");
  EXPECT_EQ(tokens[1].lexeme, "123");
}

TEST_CASE(lexer_resolves_overlap_by_rule_priority) {
  Lexer lexer = LexerBuilder()
                    .addTokenRegex("IF", "if")
                    .addTokenRegex("IDENT", "[a-zA-Z_][a-zA-Z0-9_]*")
                    .build();

  EXPECT_TRUE(!lexer.hasAmbiguity());
  std::istringstream input("if");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].name, "IF");
  EXPECT_EQ(tokens[0].lexeme, "if");
}

TEST_CASE(lexer_detects_ambiguous_accepting_states_with_same_priority) {
  Lexer lexer = LexerBuilder()
                    .addToken(TokenSpec::fromRegex("IF", "if", false, 0))
                    .addToken(TokenSpec::fromRegex(
                        "IDENT", "[a-zA-Z_][a-zA-Z0-9_]*", false, 0))
                    .build();

  EXPECT_TRUE(lexer.hasAmbiguity());
  EXPECT_TRUE(!lexer.ambiguities().empty());
  EXPECT_EQ(lexer.ambiguities()[0].token_names.size(), 2u);
}

TEST_CASE(lexer_throws_on_unrecognized_input) {
  Lexer lexer = LexerBuilder().addTokenRegex("INT", "[0-9]+").build();
  std::istringstream input("12a");

  bool thrown = false;
  try {
    lexer.tokenize(input);
  } catch (const LexerError &) {
    thrown = true;
  }

  EXPECT_TRUE(thrown);
}

TEST_CASE(lexer_can_use_explicit_automaton_rule) {
  Nfa nfa;
  int start = nfa.addState();
  int accept = nfa.addState();
  nfa.setStartState(start);
  nfa.addAcceptState(accept);
  nfa.addTransition(start, accept, static_cast<unsigned char>('='));

  Lexer lexer = LexerBuilder().addTokenAutomaton("ASSIGN", nfa).build();
  std::istringstream input("=");
  std::vector<Token> tokens = lexer.tokenize(input);

  EXPECT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].name, "ASSIGN");
  EXPECT_EQ(tokens[0].lexeme, "=");
}
