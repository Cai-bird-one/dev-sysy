#pragma once

#include "compiler/parser/parser.h"

#include <string>
#include <vector>

namespace compiler::ir {

const compiler::parser::ParseNode *
findFirst(const compiler::parser::ParseNode &node, const std::string &symbol);

const compiler::parser::ParseNode *
findDirectChild(const compiler::parser::ParseNode &node,
                const std::string &symbol);

void collectNodes(const compiler::parser::ParseNode &node,
                  const std::string &symbol,
                  std::vector<const compiler::parser::ParseNode *> &out);

bool hasNonEmptyChild(const compiler::parser::ParseNode &node,
                      const std::string &symbol);

std::string findFunctionName(const compiler::parser::ParseNode &function);
std::string findFunctionReturnType(const compiler::parser::ParseNode &function);

} // namespace compiler::ir
