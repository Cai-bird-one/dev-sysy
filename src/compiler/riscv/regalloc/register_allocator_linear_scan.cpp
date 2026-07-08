#include "compiler/riscv/regalloc/register_allocator_internal.h"

#include <algorithm>
#include <set>

namespace compiler::riscv {
namespace {

using namespace regalloc_detail;

void appendUnique(std::vector<std::string> &target,
                  const std::vector<std::string> &source) {
  for (const std::string &reg : source) {
    if (!containsRegister(target, reg)) {
      target.push_back(reg);
    }
  }
}

void appendUnique(std::vector<std::string> &target, const std::string &reg) {
  if (!reg.empty() && !containsRegister(target, reg)) {
    target.push_back(reg);
  }
}

std::string incomingArgumentRegister(
    const RegisterAllocator::LiveInterval &interval) {
  if (interval.parameter_index <= 0 || interval.parameter_index >= 8) {
    return "";
  }
  return "a" + std::to_string(interval.parameter_index);
}

std::vector<std::string>
registerPreferenceFor(const RegisterAllocator::LiveInterval &interval,
                      RegisterAllocationMode mode) {
  std::vector<std::string> registers;
  if (mode == RegisterAllocationMode::PreferIncomingArguments &&
      interval.is_parameter) {
    appendUnique(registers, incomingArgumentRegister(interval));
    appendUnique(registers, kTempRegisters);
    appendUnique(registers, kArgumentRegisters);
    appendUnique(registers, kCalleeSavedRegisters);
    return registers;
  }
  if (mode == RegisterAllocationMode::PreferCalleeSaved) {
    appendUnique(registers, kCalleeSavedRegisters);
    appendUnique(registers, kTempRegisters);
    appendUnique(registers, kArgumentRegisters);
    return registers;
  }
  if (mode == RegisterAllocationMode::PreferCallerSaved) {
    appendUnique(registers, kTempRegisters);
    appendUnique(registers, kArgumentRegisters);
    appendUnique(registers, kCalleeSavedRegisters);
    return registers;
  }
  if (interval.crosses_call) {
    appendUnique(registers, kCalleeSavedRegisters);
    appendUnique(registers, kTempRegisters);
    return registers;
  }
  if (interval.is_parameter) {
    appendUnique(registers, kTempRegisters);
    appendUnique(registers, kCalleeSavedRegisters);
    return registers;
  }
  if (interval.used_at_call) {
    appendUnique(registers, kTempRegisters);
    appendUnique(registers, kCalleeSavedRegisters);
    return registers;
  }
  appendUnique(registers, kTempRegisters);
  appendUnique(registers, kArgumentRegisters);
  appendUnique(registers, kCalleeSavedRegisters);
  return registers;
}

int nextUseAfter(const RegisterAllocator::LiveInterval &interval,
                 int position) {
  auto found =
      std::lower_bound(interval.uses.begin(), interval.uses.end(), position);
  if (found == interval.uses.end()) {
    return kNoFutureUse;
  }
  return *found;
}

} // namespace

RegisterAllocation
RegisterAllocator::linearScanAllocate(std::vector<LiveInterval> intervals,
                                      RegisterAllocationMode mode) const {
  std::sort(intervals.begin(), intervals.end(),
            [mode](const LiveInterval &left, const LiveInterval &right) {
              if (left.start != right.start) {
                return left.start < right.start;
              }
              if (mode == RegisterAllocationMode::PreferIncomingArguments &&
                  left.is_parameter != right.is_parameter) {
                return left.is_parameter && !right.is_parameter;
              }
              if (mode == RegisterAllocationMode::PreferIncomingArguments &&
                  left.is_parameter && right.is_parameter &&
                  left.parameter_index != right.parameter_index) {
                return left.parameter_index < right.parameter_index;
              }
              if (left.end != right.end) {
                return left.end > right.end;
              }
              return left.value < right.value;
            });

  std::vector<size_t> active;
  for (size_t i = 0; i < intervals.size(); ++i) {
    LiveInterval &current = intervals[i];
    active.erase(std::remove_if(active.begin(), active.end(),
                                [&](size_t active_index) {
                                  return intervals[active_index].end <
                                         current.start;
                                }),
                 active.end());

    std::vector<std::string> preferred_registers =
        registerPreferenceFor(current, mode);
    std::set<std::string> unavailable;
    for (size_t active_index : active) {
      if (intervals[active_index].assigned) {
        unavailable.insert(intervals[active_index].reg);
      }
    }

    std::string chosen;
    for (const std::string &reg : preferred_registers) {
      if (unavailable.find(reg) == unavailable.end()) {
        chosen = reg;
        break;
      }
    }
    if (!chosen.empty()) {
      current.assigned = true;
      current.reg = chosen;
      active.push_back(i);
      continue;
    }

    int current_next_use = nextUseAfter(current, current.start);
    auto spill_it = active.end();
    int latest_next_use = -1;
    int latest_end = -1;
    for (auto it = active.begin(); it != active.end(); ++it) {
      LiveInterval &candidate = intervals[*it];
      if (!candidate.assigned ||
          !containsRegister(preferred_registers, candidate.reg)) {
        continue;
      }
      int candidate_next_use = nextUseAfter(candidate, current.start);
      if (candidate_next_use > latest_next_use ||
          (candidate_next_use == latest_next_use &&
           candidate.end > latest_end)) {
        spill_it = it;
        latest_next_use = candidate_next_use;
        latest_end = candidate.end;
      }
    }

    if (spill_it != active.end() &&
        (latest_next_use > current_next_use ||
         (latest_next_use == current_next_use &&
          intervals[*spill_it].end > current.end))) {
      LiveInterval &spilled = intervals[*spill_it];
      current.assigned = true;
      current.reg = spilled.reg;
      spilled.assigned = false;
      spilled.reg.clear();
      active.erase(spill_it);
      active.push_back(i);
    }
  }

  RegisterAllocation allocation;
  for (const LiveInterval &interval : intervals) {
    if (interval.assigned) {
      allocation.assign(interval.value, interval.reg);
    }
  }
  return allocation;
}

} // namespace compiler::riscv
