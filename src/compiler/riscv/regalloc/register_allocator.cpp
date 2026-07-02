#include "compiler/riscv/regalloc/register_allocator.h"

#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

#include <algorithm>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

namespace compiler::riscv {

namespace {

const std::vector<std::string> kTempRegisters = {"t3", "t4", "t5", "t6"};
const std::vector<std::string> kArgumentRegisters = {"a1", "a2", "a3", "a4",
                                                     "a5", "a6", "a7"};
const std::vector<std::string> kCalleeSavedRegisters = {
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};

const int kNoFutureUse = std::numeric_limits<int>::max() / 4;
const int kBitsPerWord = 64;

using BitVector = std::vector<unsigned long long>;

bool isKoopaValue(const std::string &text) {
  return !text.empty() && text[0] == '%';
}

bool isCalleeSavedRegister(const std::string &reg) {
  return reg == "s1" || reg == "s2" || reg == "s3" || reg == "s4" ||
         reg == "s5" || reg == "s6" || reg == "s7" || reg == "s8" ||
         reg == "s9" || reg == "s10" || reg == "s11";
}

bool isRegisterParam(const Function &function, size_t index) {
  if (index >= 8) {
    return false;
  }
  if (index >= function.param_types.size()) {
    return true;
  }
  return function.param_types[index] == "i32" ||
         startsWith(function.param_types[index], "*");
}

bool isCallInstruction(const std::string &line,
                       const std::vector<std::string> &parts) {
  return startsWith(line, "call @") ||
         (parts.size() >= 3 && parts[1] == "=" && parts[2] == "call");
}

void addUse(std::set<std::string> &uses, const std::set<std::string> &allowed,
            const std::string &value) {
  if (allowed.find(value) != allowed.end()) {
    uses.insert(value);
  }
}

void appendAll(std::vector<std::string> &target,
               const std::vector<std::string> &source) {
  target.insert(target.end(), source.begin(), source.end());
}

bool containsRegister(const std::vector<std::string> &registers,
                      const std::string &reg) {
  return std::find(registers.begin(), registers.end(), reg) != registers.end();
}

std::vector<std::string>
registerPreferenceFor(const RegisterAllocator::LiveInterval &interval) {
  std::vector<std::string> registers;
  if (interval.crosses_call) {
    appendAll(registers, kCalleeSavedRegisters);
    appendAll(registers, kTempRegisters);
    return registers;
  }
  if (interval.is_parameter) {
    appendAll(registers, kTempRegisters);
    appendAll(registers, kCalleeSavedRegisters);
    return registers;
  }
  if (interval.used_at_call) {
    appendAll(registers, kTempRegisters);
    appendAll(registers, kCalleeSavedRegisters);
    return registers;
  }
  appendAll(registers, kTempRegisters);
  appendAll(registers, kArgumentRegisters);
  appendAll(registers, kCalleeSavedRegisters);
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

void recordPoint(RegisterAllocator::LiveInterval &interval, int position,
                 bool is_use) {
  interval.start = std::min(interval.start, position);
  interval.end = std::max(interval.end, position);
  if (is_use) {
    interval.uses.push_back(position);
  }
}

void setBit(BitVector &bits, int id) {
  bits[static_cast<size_t>(id / kBitsPerWord)] |=
      1ULL << (id % kBitsPerWord);
}

void clearBit(BitVector &bits, int id) {
  bits[static_cast<size_t>(id / kBitsPerWord)] &=
      ~(1ULL << (id % kBitsPerWord));
}

void orInto(BitVector &target, const BitVector &source) {
  for (size_t i = 0; i < target.size(); ++i) {
    target[i] |= source[i];
  }
}

bool equalBits(const BitVector &left, const BitVector &right) {
  return left == right;
}

template <typename Callback>
void forEachSetBit(const BitVector &bits, const std::vector<std::string> &values,
                   Callback callback) {
  for (size_t word = 0; word < bits.size(); ++word) {
    unsigned long long remaining = bits[word];
    while (remaining != 0) {
      int bit = __builtin_ctzll(remaining);
      size_t id = word * kBitsPerWord + static_cast<size_t>(bit);
      if (id < values.size()) {
        callback(values[id]);
      }
      remaining &= remaining - 1;
    }
  }
}

} // namespace

RegisterAllocation RegisterAllocator::allocate(const Function &function) const {
  std::set<std::string> allocatable = collectAllocatableValues(function);
  std::set<std::string> register_params = collectRegisterParamValues(function);
  std::vector<InstructionInfo> instructions =
      analyzeInstructions(function, allocatable);
  LivenessInfo liveness = analyzeLiveness(instructions);
  RegisterAllocation allocation =
      linearScanAllocate(buildLiveIntervals(function, allocatable,
                                            register_params, instructions,
                                            liveness));
  recordCallSavedValues(function, liveness, allocation);
  return allocation;
}

std::set<std::string>
RegisterAllocator::collectAllocatableValues(const Function &function) const {
  std::set<std::string> values;
  for (size_t i = 0; i < function.params.size(); ++i) {
    if (isRegisterParam(function, i)) {
      values.insert(function.params[i]);
    }
  }
  for (const std::string &line : function.instructions) {
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.size() < 3 || parts[1] != "=" || !isKoopaValue(parts[0])) {
      continue;
    }
    if (parts[2] == "alloc") {
      continue;
    }
    values.insert(parts[0]);
  }
  return values;
}

std::set<std::string>
RegisterAllocator::collectRegisterParamValues(const Function &function) const {
  std::set<std::string> values;
  for (size_t i = 0; i < function.params.size(); ++i) {
    if (isRegisterParam(function, i)) {
      values.insert(function.params[i]);
    }
  }
  return values;
}

std::vector<RegisterAllocator::InstructionInfo>
RegisterAllocator::analyzeInstructions(
    const Function &function, const std::set<std::string> &allocatable) const {
  std::vector<InstructionInfo> instructions(function.instructions.size());
  std::map<std::string, int> label_indices;
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    std::string line = trim(function.instructions[i]);
    if (!line.empty() && line.back() == ':') {
      label_indices[line.substr(0, line.size() - 1)] = static_cast<int>(i);
    }
  }

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const std::string &line = function.instructions[i];
    std::vector<std::string> parts = splitWhitespace(line);
    InstructionInfo &info = instructions[i];
    if (parts.empty()) {
      continue;
    }

    if (parts[0] == "ret") {
      if (parts.size() == 2) {
        addUse(info.uses, allocatable, parts[1]);
      }
      continue;
    }

    if (parts[0] == "jump") {
      auto found = label_indices.find(parts[1]);
      if (found != label_indices.end()) {
        info.successors.push_back(found->second);
      }
      continue;
    }

    if (parts[0] == "br") {
      addUse(info.uses, allocatable, parts[1]);
      for (size_t target = 2; target <= 3 && target < parts.size(); ++target) {
        auto found = label_indices.find(parts[target]);
        if (found != label_indices.end()) {
          info.successors.push_back(found->second);
        }
      }
      continue;
    }

    if (parts[0] == "store") {
      addUse(info.uses, allocatable, parts[1]);
      addUse(info.uses, allocatable, parts[2]);
    } else if (isCallInstruction(line, parts)) {
      CallInstruction call = parseCallInstruction(line);
      if (call.has_result && allocatable.find(call.result) != allocatable.end()) {
        info.defs.insert(call.result);
      }
      for (const std::string &arg : call.args) {
        addUse(info.uses, allocatable, arg);
      }
    } else if (parts.size() >= 3 && parts[1] == "=") {
      if (allocatable.find(parts[0]) != allocatable.end()) {
        info.defs.insert(parts[0]);
      }
      const std::string &op = parts[2];
      if (op == "load" && parts.size() == 4) {
        addUse(info.uses, allocatable, parts[3]);
      } else if ((op == "getelemptr" || op == "getptr") && parts.size() == 5) {
        addUse(info.uses, allocatable, parts[3]);
        addUse(info.uses, allocatable, parts[4]);
      } else if (parts.size() == 5) {
        addUse(info.uses, allocatable, parts[3]);
        addUse(info.uses, allocatable, parts[4]);
      }
    }

    if (i + 1 < function.instructions.size()) {
      info.successors.push_back(static_cast<int>(i + 1));
    }
  }

  return instructions;
}

RegisterAllocator::LivenessInfo RegisterAllocator::analyzeLiveness(
    const std::vector<InstructionInfo> &instructions) const {
  std::unordered_map<std::string, int> value_ids;
  std::vector<std::string> values;
  auto internValue = [&](const std::string &value) {
    auto found = value_ids.find(value);
    if (found != value_ids.end()) {
      return found->second;
    }
    int id = static_cast<int>(values.size());
    value_ids[value] = id;
    values.push_back(value);
    return id;
  };
  for (const InstructionInfo &instruction : instructions) {
    for (const std::string &value : instruction.uses) {
      internValue(value);
    }
    for (const std::string &value : instruction.defs) {
      internValue(value);
    }
  }

  size_t word_count =
      (values.size() + static_cast<size_t>(kBitsPerWord - 1)) /
      static_cast<size_t>(kBitsPerWord);
  std::vector<BitVector> live_in_bits(instructions.size(),
                                      BitVector(word_count, 0));
  std::vector<BitVector> live_out_bits(instructions.size(),
                                       BitVector(word_count, 0));
  BitVector next_out(word_count, 0);
  BitVector next_in(word_count, 0);
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t offset = 0; offset < instructions.size(); ++offset) {
      size_t i = instructions.size() - 1 - offset;
      std::fill(next_out.begin(), next_out.end(), 0);
      for (int successor : instructions[i].successors) {
        orInto(next_out, live_in_bits[successor]);
      }
      next_in = next_out;
      for (const std::string &value : instructions[i].defs) {
        clearBit(next_in, value_ids[value]);
      }
      for (const std::string &value : instructions[i].uses) {
        setBit(next_in, value_ids[value]);
      }
      if (!equalBits(next_in, live_in_bits[i]) ||
          !equalBits(next_out, live_out_bits[i])) {
        live_in_bits[i] = next_in;
        live_out_bits[i] = next_out;
        changed = true;
      }
    }
  }

  LivenessInfo result;
  result.values = std::move(values);
  result.live_in = std::move(live_in_bits);
  result.live_out = std::move(live_out_bits);
  return result;
}

std::vector<RegisterAllocator::LiveInterval>
RegisterAllocator::buildLiveIntervals(
    const Function &function, const std::set<std::string> &allocatable,
    const std::set<std::string> &register_params,
    const std::vector<InstructionInfo> &instructions,
    const LivenessInfo &liveness) const {
  std::map<std::string, LiveInterval> intervals;
  for (const std::string &value : allocatable) {
    LiveInterval interval;
    interval.value = value;
    interval.start = kNoFutureUse;
    interval.end = -1;
    interval.is_parameter = register_params.find(value) != register_params.end();
    intervals[value] = std::move(interval);
  }

  for (const std::string &param : register_params) {
    auto found = intervals.find(param);
    if (found != intervals.end()) {
      recordPoint(found->second, 0, false);
    }
  }

  for (size_t i = 0; i < instructions.size(); ++i) {
    int use_position = static_cast<int>(i * 2);
    int def_position = use_position + 1;
    forEachSetBit(liveness.live_in[i], liveness.values,
                  [&](const std::string &value) {
                    recordPoint(intervals[value], use_position, false);
                  });
    forEachSetBit(liveness.live_out[i], liveness.values,
                  [&](const std::string &value) {
                    recordPoint(intervals[value], def_position, false);
                  });
    for (const std::string &value : instructions[i].uses) {
      recordPoint(intervals[value], use_position, true);
    }
    for (const std::string &value : instructions[i].defs) {
      recordPoint(intervals[value], def_position, false);
    }

    std::vector<std::string> parts = splitWhitespace(function.instructions[i]);
    if (!isCallInstruction(function.instructions[i], parts)) {
      continue;
    }
    for (const std::string &value : instructions[i].uses) {
      intervals[value].used_at_call = true;
    }
    forEachSetBit(liveness.live_out[i], liveness.values,
                  [&](const std::string &value) {
                    intervals[value].crosses_call = true;
                  });
  }

  std::vector<LiveInterval> result;
  for (auto &[_, interval] : intervals) {
    if (interval.end < interval.start || interval.uses.empty()) {
      continue;
    }
    std::sort(interval.uses.begin(), interval.uses.end());
    interval.uses.erase(std::unique(interval.uses.begin(), interval.uses.end()),
                        interval.uses.end());
    result.push_back(std::move(interval));
  }
  return result;
}

RegisterAllocation RegisterAllocator::linearScanAllocate(
    std::vector<LiveInterval> intervals) const {
  std::sort(intervals.begin(), intervals.end(),
            [](const LiveInterval &left, const LiveInterval &right) {
              if (left.start != right.start) {
                return left.start < right.start;
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
        registerPreferenceFor(current);
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

void RegisterAllocator::recordCallSavedValues(
    const Function &function, const LivenessInfo &liveness,
    RegisterAllocation &allocation) const {
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const std::string &line = function.instructions[i];
    std::vector<std::string> parts = splitWhitespace(line);
    if (!isCallInstruction(line, parts)) {
      continue;
    }
    CallInstruction call = parseCallInstruction(line);
    forEachSetBit(liveness.live_out[i], liveness.values,
                  [&](const std::string &value) {
      if (call.has_result && value == call.result) {
        return;
      }
      if (allocation.hasRegister(value) &&
          !isCalleeSavedRegister(allocation.registerFor(value))) {
        allocation.addCallSavedValue(i, value);
      }
                  });
  }
}

} // namespace compiler::riscv
