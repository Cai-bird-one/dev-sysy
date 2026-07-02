#pragma once

#include "compiler/ir/opt/analysis/control_flow_graph.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace compiler::ir::opt {

struct LivenessInfo {
  std::vector<std::set<std::string>> live_in;
  std::vector<std::set<std::string>> live_out;
};

struct DenseLivenessInfo {
  std::vector<std::string> values;
  std::unordered_map<std::string, int> value_ids;
  std::vector<std::vector<unsigned long long>> live_in;
  std::vector<std::vector<unsigned long long>> live_out;
};

LivenessInfo analyzeLiveness(const IrFunction &function,
                             const ControlFlowGraph &cfg);
LivenessInfo analyzeLiveness(const IrFunction &function);
DenseLivenessInfo analyzeDenseLiveness(const IrFunction &function,
                                       const ControlFlowGraph &cfg);
DenseLivenessInfo analyzeDenseLiveness(const IrFunction &function);
bool isLiveOut(const DenseLivenessInfo &info, size_t instruction,
               const std::string &value);

} // namespace compiler::ir::opt
