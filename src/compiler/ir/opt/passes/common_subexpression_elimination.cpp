#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace compiler::ir::opt {
namespace {

bool isPureExpression(const Assignment &assignment) {
  return assignment.op == "add" || assignment.op == "sub" ||
         assignment.op == "mul" || assignment.op == "div" ||
         assignment.op == "mod" || assignment.op == "eq" ||
         assignment.op == "ne" || assignment.op == "lt" ||
         assignment.op == "gt" || assignment.op == "le" ||
         assignment.op == "ge";
}

bool isCommutative(const std::string &op) {
  return op == "add" || op == "mul" || op == "eq" || op == "ne";
}

std::string expressionKey(Assignment assignment) {
  if (assignment.args.size() == 2 && isCommutative(assignment.op) &&
      assignment.args[1] < assignment.args[0]) {
    std::swap(assignment.args[0], assignment.args[1]);
  }

  std::string key = assignment.op;
  for (const std::string &arg : assignment.args) {
    key += "|" + arg;
  }
  return key;
}

std::string resolve(const std::string &operand,
                    const std::map<std::string, std::string> &values) {
  auto found = values.find(operand);
  return found == values.end() ? operand : found->second;
}

} // namespace

PassResult CommonSubexpressionEliminationPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> used_outside =
      collectValuesUsedOutsideDefiningBlock(function);
  std::map<std::string, std::string> expressions;
  std::map<std::string, std::string> values;
  std::vector<std::string> optimized;

  for (std::string line : function.instructions) {
    if (isLabel(line)) {
      expressions.clear();
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
        expressions.clear();
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

    if (isPureExpression(assignment)) {
      std::string key = expressionKey(assignment);
      auto found = expressions.find(key);
      if (found != expressions.end()) {
        if (used_outside.find(assignment.result) == used_outside.end()) {
          values[assignment.result] = found->second;
          result.changed = true;
          continue;
        }
      }
      expressions[key] = assignment.result;
    }

    optimized.push_back(formatAssignment(assignment));
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
