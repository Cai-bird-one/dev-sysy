#include "compiler/lexer/token_rules.h"

namespace compiler::lexer {
namespace {

const std::vector<RegexTokenRule> kDefaultRegexTokenRules = {
    // Whitespace and comments.
    {"UTF8_BOM", "\xEF\xBB\xBF", TokenRuleAction::Skip},
    {"WHITESPACE", "[ \\t\\n\\r\\v\\f]+", TokenRuleAction::Skip},
    {"LINE_COMMENT", "//.*", TokenRuleAction::Skip},
    {"BLOCK_COMMENT", "/[*]([^*]|[*]+[^*/])*[*]+/", TokenRuleAction::Skip},

    // Keywords must appear before IDENT, matching Flex rule priority.
    {"CONST", "const"},
    {"VOID", "void"},
    {"INT", "int"},
    {"IF", "if"},
    {"ELSE", "else"},
    {"WHILE", "while"},
    {"BREAK", "break"},
    {"CONTINUE", "continue"},
    {"RETURN", "return"},

    // Identifier:
    // identifier ::= identifier-nondigit
    //              | identifier identifier-nondigit
    //              | identifier digit
    {"IDENT", "[a-zA-Z_][a-zA-Z0-9_]*"},

    // Invalid integer-like strings must be checked before valid integer
    // literals so longest-match tokenization reports them as one lexer error.
    {"INVALID_INT_CONST", "0[xX]"},
    {"INVALID_INT_CONST", "0[xX][0-9a-fA-F]*[g-zG-Z_][a-zA-Z0-9_]*"},
    {"INVALID_INT_CONST", "0[0-7]*[8-9][0-9a-zA-Z_]*"},
    {"INVALID_INT_CONST", "[1-9][0-9]*[a-zA-Z_][a-zA-Z0-9_]*"},

    // Integer literals:
    // integer-const     ::= decimal-const | octal-const | hexadecimal-const
    // decimal-const     ::= nonzero-digit | decimal-const digit
    // octal-const       ::= "0" | octal-const octal-digit
    // hexadecimal-const ::= hexadecimal-prefix hexadecimal-digit
    //                    | hexadecimal-const hexadecimal-digit
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

    {"LE", "<="},
    {"GE", ">="},
    {"EQ", "=="},
    {"NE", "!="},
    {"AND", "&&"},
    {"OR", "[|][|]"},
    {"LT", "<"},
    {"GT", ">"},
    {"ASSIGN", "="},
    {"SEMICOLON", ";"},
    {"COMMA", ","},
    {"LPAREN", "[(]"},
    {"RPAREN", "[)]"},
    {"LBRACKET", "\\["},
    {"RBRACKET", "\\]"},
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
