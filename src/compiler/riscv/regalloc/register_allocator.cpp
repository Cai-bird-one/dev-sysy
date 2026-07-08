#include "compiler/riscv/regalloc/register_allocator.h"

#include "compiler/riscv/regalloc/register_allocator_internal.h"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

namespace compiler::riscv {

using namespace regalloc_detail;

RegisterAllocation RegisterAllocator::allocate(const Function &function) const {
  std::set<std::string> allocatable = collectAllocatableValues(function);
  std::set<std::string> register_params = collectRegisterParamValues(function);
  std::vector<InstructionInfo> instructions =
      analyzeInstructions(function, allocatable);
  LivenessInfo liveness = analyzeLiveness(function);
  std::vector<LiveInterval> intervals = buildLiveIntervals(
      function, allocatable, register_params, instructions, liveness);
  return allocateMultiRound(function, intervals, liveness);
}

RegisterAllocation RegisterAllocator::allocateMultiRound(
    const Function &function, const std::vector<LiveInterval> &intervals,
    const LivenessInfo &liveness) const {
  const std::vector<RegisterAllocationMode> modes = {
      RegisterAllocationMode::Default,
      RegisterAllocationMode::PreferIncomingArguments,
      RegisterAllocationMode::PreferCalleeSaved,
      RegisterAllocationMode::PreferCallerSaved,
  };

  RegisterAllocation best;
  long long best_score = std::numeric_limits<long long>::max();
  bool has_best = false;
  for (RegisterAllocationMode mode : modes) {
    RegisterAllocation candidate = linearScanAllocate(intervals, mode);
    recordCallSavedValues(function, liveness, candidate);
    long long score = scoreAllocation(function, intervals, liveness, candidate);
    if (!has_best || score < best_score) {
      best = candidate;
      best_score = score;
      has_best = true;
    }
  }
  return best;
}

long long RegisterAllocator::scoreAllocation(
    const Function &function, const std::vector<LiveInterval> &intervals,
    const LivenessInfo &, const RegisterAllocation &allocation) const {
  long long score = 0;
  std::set<std::string> used_callee_saved;
  for (const LiveInterval &interval : intervals) {
    if (!allocation.hasRegister(interval.value)) {
      score += 1000;
      score += static_cast<long long>(interval.uses.size()) * 50;
      score += std::max(0, interval.end - interval.start);
      continue;
    }

    const std::string &reg = allocation.registerFor(interval.value);
    if (isCalleeSavedRegister(reg)) {
      used_callee_saved.insert(reg);
    } else if (interval.crosses_call) {
      score += 30;
    }
    if (interval.parameter_index > 0 && interval.parameter_index < 8) {
      std::string incoming = "a" + std::to_string(interval.parameter_index);
      if (reg != incoming) {
        score += 3;
      }
    }
  }

  score += static_cast<long long>(used_callee_saved.size()) * 8;
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    score += static_cast<long long>(allocation.callSavedValues(i).size()) * 12;
  }
  return score;
}

} // namespace compiler::riscv
