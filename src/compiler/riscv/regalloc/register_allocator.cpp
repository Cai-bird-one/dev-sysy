#include "compiler/riscv/regalloc/register_allocator.h"

#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

#include <algorithm>
#include <utility>

namespace compiler::riscv {

namespace {

const std::vector<std::string> kAllocatableRegisters = {
    "t3", "t4", "t5", "t6", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};
const std::vector<std::string> kParameterRegisters = {"t3", "t4", "t5", "t6"};

bool isKoopaValue(const std::string &text) {
  return !text.empty() && text[0] == '%';
}

bool isCalleeSavedRegister(const std::string &reg) {
  return reg == "s1" || reg == "s2" || reg == "s3" || reg == "s4" ||
         reg == "s5" || reg == "s6" || reg == "s7" || reg == "s8" ||
         reg == "s9" || reg == "s10" || reg == "s11";
}

bool isScalarRegisterParam(const Function &function, size_t index) {
  if (index >= 8) {
    return false;
  }
  if (index >= function.param_types.size()) {
    return true;
  }
  return function.param_types[index] == "i32";
}

void addUse(std::set<std::string> &uses, const std::set<std::string> &allowed,
            const std::string &value) {
  if (allowed.find(value) != allowed.end()) {
    uses.insert(value);
  }
}

void addEdge(std::map<std::string, std::set<std::string>> &graph,
             const std::string &left, const std::string &right) {
  if (left == right) {
    return;
  }
  graph[left].insert(right);
  graph[right].insert(left);
}

std::set<std::string> setUnion(const std::set<std::string> &left,
                               const std::set<std::string> &right) {
  std::set<std::string> result = left;
  result.insert(right.begin(), right.end());
  return result;
}

std::set<std::string> withoutDefs(std::set<std::string> values,
                                  const std::set<std::string> &defs) {
  for (const std::string &def : defs) {
    values.erase(def);
  }
  return values;
}

} // namespace

RegisterAllocation RegisterAllocator::allocate(const Function &function) const {
  std::set<std::string> allocatable = collectAllocatableValues(function);
  std::set<std::string> register_params = collectRegisterParamValues(function);
  std::vector<InstructionInfo> instructions =
      analyzeInstructions(function, allocatable);
  std::vector<std::set<std::string>> live_in;
  std::vector<std::set<std::string>> live_out;
  RegisterAllocation allocation =
      colorGraph(buildInterferenceGraph(allocatable, instructions, live_in,
                                        live_out),
                 register_params);
  recordCallSavedValues(function, live_out, allocation);
  return allocation;
}

std::set<std::string>
RegisterAllocator::collectAllocatableValues(const Function &function) const {
  std::set<std::string> values;
  for (size_t i = 0; i < function.params.size(); ++i) {
    if (isScalarRegisterParam(function, i)) {
      values.insert(function.params[i]);
    }
  }
  for (const std::string &line : function.instructions) {
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.size() < 3 || parts[1] != "=" || !isKoopaValue(parts[0])) {
      continue;
    }
    const std::string &op = parts[2];
    if (op == "alloc" || op == "getelemptr" || op == "getptr") {
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
    if (isScalarRegisterParam(function, i)) {
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
    } else if (startsWith(line, "call @") ||
               (parts.size() >= 3 && parts[1] == "=" &&
                parts[2] == "call")) {
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

std::map<std::string, std::set<std::string>>
RegisterAllocator::buildInterferenceGraph(
    const std::set<std::string> &allocatable,
    const std::vector<InstructionInfo> &instructions,
    std::vector<std::set<std::string>> &live_in,
    std::vector<std::set<std::string>> &live_out) const {
  std::map<std::string, std::set<std::string>> graph;
  for (const std::string &value : allocatable) {
    graph[value];
  }

  live_in.assign(instructions.size(), {});
  live_out.assign(instructions.size(), {});
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t offset = 0; offset < instructions.size(); ++offset) {
      size_t i = instructions.size() - 1 - offset;
      std::set<std::string> next_out;
      for (int successor : instructions[i].successors) {
        next_out = setUnion(next_out, live_in[successor]);
      }
      std::set<std::string> next_in =
          setUnion(instructions[i].uses,
                   withoutDefs(next_out, instructions[i].defs));
      if (next_in != live_in[i] || next_out != live_out[i]) {
        live_in[i] = std::move(next_in);
        live_out[i] = std::move(next_out);
        changed = true;
      }
    }
  }

  for (size_t i = 0; i < instructions.size(); ++i) {
    for (const std::string &def : instructions[i].defs) {
      for (const std::string &live : live_out[i]) {
        addEdge(graph, def, live);
      }
    }
  }
  if (!live_in.empty()) {
    for (const std::string &left : live_in.front()) {
      for (const std::string &right : live_in.front()) {
        addEdge(graph, left, right);
      }
    }
  }
  return graph;
}

void RegisterAllocator::recordCallSavedValues(
    const Function &function, const std::vector<std::set<std::string>> &live_out,
    RegisterAllocation &allocation) const {
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const std::string &line = function.instructions[i];
    std::vector<std::string> parts = splitWhitespace(line);
    if (!(startsWith(line, "call @") ||
          (parts.size() >= 3 && parts[1] == "=" && parts[2] == "call"))) {
      continue;
    }
    CallInstruction call = parseCallInstruction(line);
    for (const std::string &value : live_out[i]) {
      if (call.has_result && value == call.result) {
        continue;
      }
      if (allocation.hasRegister(value) &&
          !isCalleeSavedRegister(allocation.registerFor(value))) {
        allocation.addCallSavedValue(i, value);
      }
    }
  }
}

RegisterAllocation RegisterAllocator::colorGraph(
    const std::map<std::string, std::set<std::string>> &graph,
    const std::set<std::string> &register_params) const {
  RegisterAllocation allocation;
  std::vector<std::pair<std::string, size_t>> order;
  for (const auto &[value, neighbors] : graph) {
    order.push_back({value, neighbors.size()});
  }
  std::sort(order.begin(), order.end(),
            [](const auto &left, const auto &right) {
              if (left.second != right.second) {
                return left.second > right.second;
              }
              return left.first < right.first;
            });

  std::map<std::string, std::string> assigned;
  for (const auto &[value, _] : order) {
    std::set<std::string> unavailable;
    auto found = graph.find(value);
    if (found != graph.end()) {
      for (const std::string &neighbor : found->second) {
        auto assigned_neighbor = assigned.find(neighbor);
        if (assigned_neighbor != assigned.end()) {
          unavailable.insert(assigned_neighbor->second);
        }
      }
    }

    const std::vector<std::string> &available_registers =
        register_params.find(value) == register_params.end()
            ? kAllocatableRegisters
            : kParameterRegisters;
    for (const std::string &reg : available_registers) {
      if (unavailable.find(reg) == unavailable.end()) {
        assigned[value] = reg;
        allocation.assign(value, reg);
        break;
      }
    }
  }
  return allocation;
}

} // namespace compiler::riscv
