#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <utility>

namespace compiler::ir::opt {
namespace {

std::set<std::string> collectScalarAllocs(const IrFunction &function) {
  std::set<std::string> allocs;
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && assignment.op == "alloc" &&
        assignment.args.size() == 1 && assignment.args[0] == "i32") {
      allocs.insert(assignment.result);
    }
  }
  return allocs;
}

std::string resolve(const std::string &operand,
                    const std::map<std::string, std::string> &values) {
  auto found = values.find(operand);
  return found == values.end() ? operand : found->second;
}

std::vector<std::pair<std::string, std::string>>
replacements(const std::map<std::string, std::string> &values) {
  return {values.begin(), values.end()};
}

bool isCall(const Assignment &assignment, const std::vector<std::string> &parts) {
  return (assignment.valid && assignment.op == "call") ||
         (!parts.empty() && parts[0] == "call");
}

} // namespace

PassResult LocalLoadStoreForwardingPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> scalar_allocs = collectScalarAllocs(function);
  std::set<std::string> used_outside =
      collectValuesUsedOutsideDefiningBlock(function);
  std::map<std::string, std::string> memory_values;
  std::map<std::string, std::string> values;
  std::vector<std::string> optimized;

  for (std::string line : function.instructions) {
    if (isLabel(line)) {
      memory_values.clear();
      values.clear();
      optimized.push_back(std::move(line));
      continue;
    }

    std::string replaced = replaceOperands(line, replacements(values));
    if (replaced != line) {
      line = std::move(replaced);
      result.changed = true;
    }

    std::vector<std::string> parts = splitWhitespace(line);
    Assignment assignment = parseAssignment(line);
    if (isCall(assignment, parts)) {
      memory_values.clear();
      optimized.push_back(std::move(line));
      continue;
    }

    if (parts.size() == 3 && parts[0] == "store") {
      std::string value = resolve(parts[1], values);
      const std::string &pointer = parts[2];
      if (scalar_allocs.find(pointer) != scalar_allocs.end()) {
        memory_values[pointer] = value;
      } else {
        memory_values.clear();
      }
      optimized.push_back("store " + value + ", " + pointer);
      continue;
    }

    if (assignment.valid && assignment.op == "load" &&
        assignment.args.size() == 1) {
      const std::string &pointer = assignment.args[0];
      auto found = memory_values.find(pointer);
      if (found != memory_values.end()) {
        if (used_outside.find(assignment.result) == used_outside.end()) {
          values[assignment.result] = found->second;
          result.changed = true;
          continue;
        }
      }
    }

    if (assignment.valid) {
      values.erase(assignment.result);
    } else if (isTerminator(line)) {
      memory_values.clear();
      values.clear();
    }
    optimized.push_back(std::move(line));
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
