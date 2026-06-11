#pragma once

#include "compiler/ir/opt/analysis/control_flow_graph.h"

#include <set>
#include <string>
#include <vector>

namespace compiler::ir::opt {

struct LivenessInfo {
  std::vector<std::set<std::string>> live_in;
  std::vector<std::set<std::string>> live_out;
};

LivenessInfo analyzeLiveness(const IrFunction &function,
                             const ControlFlowGraph &cfg);
LivenessInfo analyzeLiveness(const IrFunction &function);

} // namespace compiler::ir::opt
