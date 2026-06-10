#pragma once

#include "compiler/parser/parser.h"

#include <string>

namespace compiler::ir {

std::string generateKoopaProgram(const compiler::parser::ParseNode &ast);

} // namespace compiler::ir
