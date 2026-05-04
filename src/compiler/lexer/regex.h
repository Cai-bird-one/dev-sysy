#pragma once

#include "compiler/lexer/lexer.h"

#include <string>

namespace compiler::lexer {

Nfa regexToNfa(std::string regex);

} // namespace compiler::lexer
