#pragma once

#include "compiler/lexer/lexer.h"

#include <vector>

namespace compiler::lexer {

enum class TokenRuleAction {
  Emit,
  Skip,
};

struct RegexTokenRule {
  const char *name;
  const char *regex;
  TokenRuleAction action = TokenRuleAction::Emit;
};

const std::vector<RegexTokenRule> &defaultRegexTokenRules();

LexerBuilder defaultLexerBuilder();
Lexer buildDefaultLexer();

} // namespace compiler::lexer
