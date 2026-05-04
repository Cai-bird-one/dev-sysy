#pragma once

#include "compiler/lexer/lexer.h"

#include <vector>

namespace compiler::lexer {

Lexer buildLexerAutomaton(const std::vector<TokenSpec> &tokens);

} // namespace compiler::lexer
