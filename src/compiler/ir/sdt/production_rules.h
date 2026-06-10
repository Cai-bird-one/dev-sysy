#pragma once

#include "compiler/ir/sdt/sdt.h"

#include <initializer_list>
#include <string>

namespace compiler::ir::sdt {

int findProductionId(const std::string &lhs,
                     std::initializer_list<std::string> rhs);

void registerProductionRule(SyntaxDirectedTranslator &translator,
                            const std::string &lhs,
                            std::initializer_list<std::string> rhs,
                            SyntaxDirectedTranslator::Rule rule);

} // namespace compiler::ir::sdt
