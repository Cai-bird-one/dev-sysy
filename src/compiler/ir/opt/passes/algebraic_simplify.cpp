#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <vector>

namespace compiler::ir::opt {
namespace {

std::string resolve(const std::string &operand,
                    const std::map<std::string, std::string> &values) {
  auto found = values.find(operand);
  return found == values.end() ? operand : found->second;
}

bool simplify(const Assignment &assignment, std::string &replacement) {
  if (assignment.args.size() != 2) {
    return false;
  }
  const std::string &lhs = assignment.args[0];
  const std::string &rhs = assignment.args[1];
  if ((assignment.op == "add" && rhs == "0") ||
      (assignment.op == "sub" && rhs == "0") ||
      (assignment.op == "mul" && rhs == "1") ||
      (assignment.op == "div" && rhs == "1")) {
    replacement = lhs;
    return true;
  }
  if ((assignment.op == "add" && lhs == "0") ||
      (assignment.op == "mul" && lhs == "1")) {
    replacement = rhs;
    return true;
  }
  if (assignment.op == "mul" && (lhs == "0" || rhs == "0")) {
    replacement = "0";
    return true;
  }
  return false;
}

} // namespace

PassResult AlgebraicSimplifyPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> used_outside =
      collectValuesUsedOutsideDefiningBlock(function);
  std::map<std::string, std::string> values;
  std::vector<std::string> optimized;

  for (std::string line : function.instructions) {
    if (isLabel(line)) {
      values.clear();
      optimized.push_back(std::move(line));
      continue;
    }

    std::string replaced = replaceOperands(line, values);
    if (replaced != line) {
      line = std::move(replaced);
      result.changed = true;
    }

    Assignment assignment = parseAssignment(line);
    if (!assignment.valid) {
      if (isTerminator(line)) {
        values.clear();
      }
      optimized.push_back(std::move(line));
      continue;
    }

    for (std::string &arg : assignment.args) {
      std::string resolved = resolve(arg, values);
      if (resolved != arg) {
        arg = std::move(resolved);
        result.changed = true;
      }
    }

    std::string replacement;
    if (simplify(assignment, replacement)) {
      values[assignment.result] = replacement;
      if (used_outside.find(assignment.result) == used_outside.end()) {
        result.changed = true;
        continue;
      }
    }

    values.erase(assignment.result);
    optimized.push_back(formatAssignment(assignment));
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
