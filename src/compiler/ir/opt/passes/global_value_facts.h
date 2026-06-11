#pragma once

#include "compiler/ir/opt/analysis/control_flow_graph.h"
#include "compiler/ir/opt/model/ir_module.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace compiler::ir::opt {

using ValueFacts = std::map<std::string, std::string>;

std::string resolveValueFact(const std::string &value,
                             const ValueFacts &facts);
std::vector<std::pair<std::string, std::string>>
valueFactReplacements(const ValueFacts &facts);
Assignment assignmentWithResolvedValueFacts(Assignment assignment,
                                           const ValueFacts &facts);
void updateValueFactsForAssignment(const Assignment &assignment,
                                   ValueFacts &facts);
std::vector<ValueFacts>
computeBlockEntryValueFacts(const IrFunction &function,
                            const ControlFlowGraph &cfg);

} // namespace compiler::ir::opt
