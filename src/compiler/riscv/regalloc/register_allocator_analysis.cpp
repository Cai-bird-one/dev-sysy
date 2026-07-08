#include "compiler/riscv/regalloc/register_allocator_internal.h"

#include "compiler/riscv/util/riscv_utils.h"

#include <algorithm>
#include <map>
#include <utility>

namespace compiler::riscv {

using namespace regalloc_detail;

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

    if (parts[0] == "br") {
      addUse(info.uses, allocatable, parts[1]);
      continue;
    }

    if (parts[0] == "jump") {
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
  }

  return instructions;
}

RegisterAllocator::LivenessInfo
RegisterAllocator::analyzeLiveness(const Function &function) const {
  compiler::ir::opt::IrFunction ir_function;
  ir_function.instructions = function.instructions;
  return compiler::ir::opt::analyzeDenseLiveness(ir_function);
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
    if (interval.is_parameter) {
      auto param_found =
          std::find(function.params.begin(), function.params.end(), value);
      if (param_found != function.params.end()) {
        interval.parameter_index =
            static_cast<int>(param_found - function.params.begin());
      }
    }
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
                    auto found = intervals.find(value);
                    if (found != intervals.end()) {
                      recordPoint(found->second, use_position, false);
                    }
                  });
    forEachSetBit(liveness.live_out[i], liveness.values,
                  [&](const std::string &value) {
                    auto found = intervals.find(value);
                    if (found != intervals.end()) {
                      recordPoint(found->second, def_position, false);
                    }
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
                    auto found = intervals.find(value);
                    if (found != intervals.end()) {
                      found->second.crosses_call = true;
                    }
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

} // namespace compiler::riscv
