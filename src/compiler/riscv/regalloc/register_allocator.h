#pragma once

#include "compiler/riscv/model/koopa_program.h"
#include "compiler/riscv/regalloc/register_allocation.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::riscv {

class RegisterAllocator {
public:
  RegisterAllocation allocate(const Function &function) const;
  struct InstructionInfo {
    std::set<std::string> defs;
    std::set<std::string> uses;
    std::vector<int> successors;
  };

  struct LiveInterval {
    std::string value;
    int start = 0;
    int end = 0;
    std::vector<int> uses;
    bool is_parameter = false;
    bool used_at_call = false;
    bool crosses_call = false;
    bool assigned = false;
    std::string reg;
  };

  struct LivenessInfo {
    std::vector<std::string> values;
    std::vector<std::vector<unsigned long long>> live_in;
    std::vector<std::vector<unsigned long long>> live_out;
  };

private:
  std::set<std::string> collectAllocatableValues(const Function &function) const;
  std::set<std::string> collectRegisterParamValues(
      const Function &function) const;
  std::vector<InstructionInfo>
  analyzeInstructions(const Function &function,
                      const std::set<std::string> &allocatable) const;
  LivenessInfo
  analyzeLiveness(const std::vector<InstructionInfo> &instructions) const;
  std::vector<LiveInterval> buildLiveIntervals(
      const Function &function, const std::set<std::string> &allocatable,
      const std::set<std::string> &register_params,
      const std::vector<InstructionInfo> &instructions,
      const LivenessInfo &liveness) const;
  RegisterAllocation
  linearScanAllocate(std::vector<LiveInterval> intervals) const;
  void recordCallSavedValues(const Function &function,
                             const LivenessInfo &liveness,
                             RegisterAllocation &allocation) const;
};

} // namespace compiler::riscv
