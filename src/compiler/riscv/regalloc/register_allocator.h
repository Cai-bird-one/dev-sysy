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

private:
  struct InstructionInfo {
    std::set<std::string> defs;
    std::set<std::string> uses;
    std::vector<int> successors;
  };

  std::set<std::string> collectAllocatableValues(const Function &function) const;
  std::set<std::string> collectRegisterParamValues(
      const Function &function) const;
  std::vector<InstructionInfo>
  analyzeInstructions(const Function &function,
                      const std::set<std::string> &allocatable) const;
  std::map<std::string, std::set<std::string>>
  buildInterferenceGraph(const std::set<std::string> &allocatable,
                         const std::vector<InstructionInfo> &instructions,
                         std::vector<std::set<std::string>> &live_in,
                         std::vector<std::set<std::string>> &live_out) const;
  RegisterAllocation
  colorGraph(const std::map<std::string, std::set<std::string>> &graph,
             const std::set<std::string> &register_params) const;
  void recordCallSavedValues(const Function &function,
                             const std::vector<std::set<std::string>> &live_out,
                             RegisterAllocation &allocation) const;
};

} // namespace compiler::riscv
