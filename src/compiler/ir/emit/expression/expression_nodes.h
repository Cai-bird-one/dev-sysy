#pragma once

#include <string>

namespace compiler::ir {

bool isExpressionWrapper(const std::string &symbol);
bool isBinaryExpression(const std::string &symbol);
std::string binaryTailSymbol(const std::string &symbol);

} // namespace compiler::ir
