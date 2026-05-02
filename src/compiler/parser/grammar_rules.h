#pragma once

#include "compiler/parser/parser.h"

namespace compiler::parser {

const Grammar &defaultGrammar();

ParserBuilder defaultParserBuilder();
Parser buildDefaultParser();

} // namespace compiler::parser
