#include "compiler/lexer/token_rules.h"

namespace compiler::lexer {
namespace {

const std::vector<RegexTokenRule> kDefaultRegexTokenRules = {
    // Whitespace and comments.
    {"WHITESPACE", "[ \\t\\n\\r]+", TokenRuleAction::Skip},
    {"LINE_COMMENT", "//.*", TokenRuleAction::Skip},
    {"BLOCK_COMMENT", "/[*]([^*]|[*][^/])*[*]/", TokenRuleAction::Skip},

    // Keywords must appear before IDENT, matching Flex rule priority.
    {"CONST", "const"},
    {"INT", "int"},
    {"RETURN", "return"},

    // Identifier.
    {"IDENT", "[a-zA-Z_][a-zA-Z0-9_]*"},

    // Integer literals.
    {"INT_CONST", "[1-9][0-9]*"},
    {"INT_CONST", "0[0-7]*"},
    {"INT_CONST", "0[xX][0-9a-fA-F]+"},

    // Operators and separators.
    {"PLUS", "[+]"},
    {"MINUS", "-"},
    {"NOT", "!"},
    {"STAR", "[*]"},
    {"SLASH", "/"},
    {"PERCENT", "%"},

    {"ASSIGN", "="},
    {"SEMICOLON", ";"},
    {"COMMA", ","},
    {"LPAREN", "[(]"},
    {"RPAREN", "[)]"},
    {"LBRACE", "[{]"},
    {"RBRACE", "[}]"},

    // Fallback rule, similar to Flex's "." rule.
    {"CHAR", "."},
};

std::set<std::string> buildDefaultTokenNames() {
  std::set<std::string> names;
  for (const RegexTokenRule &rule : kDefaultRegexTokenRules) {
    if (rule.action == TokenRuleAction::Emit) {
      names.insert(rule.name);
    }
  }
  return names;
}

const std::set<std::string> kDefaultTokenNames = buildDefaultTokenNames();

} // namespace

const std::vector<RegexTokenRule> &defaultRegexTokenRules() {
  return kDefaultRegexTokenRules;
}

const std::set<std::string> &defaultTokenNames() { return kDefaultTokenNames; }

LexerBuilder defaultLexerBuilder() {
  LexerBuilder builder;
  for (const RegexTokenRule &rule : defaultRegexTokenRules()) {
    if (rule.action == TokenRuleAction::Skip) {
      builder.skipRegex(rule.name, rule.regex);
    } else {
      builder.addTokenRegex(rule.name, rule.regex);
    }
  }
  return builder;
}

Lexer buildDefaultLexer() { return defaultLexerBuilder().build(); }

} // namespace compiler::lexer
