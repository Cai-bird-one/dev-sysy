#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <vector>

namespace compiler::ir::opt {
namespace {

bool isBinaryOp(const std::string &op) {
  return op == "add" || op == "sub" || op == "mul" || op == "div" ||
         op == "mod" || op == "eq" || op == "ne" || op == "lt" ||
         op == "gt" || op == "le" || op == "ge";
}

std::string resolve(const std::string &operand,
                    const std::map<std::string, std::string> &constants) {
  auto found = constants.find(operand);
  return found == constants.end() ? operand : found->second;
}

} // namespace

PassResult ConstantFoldingPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> used_outside =
      collectValuesUsedOutsideDefiningBlock(function);
  std::map<std::string, std::string> constants;
  std::vector<std::string> optimized;

  for (std::string line : function.instructions) {
    if (isLabel(line)) {
      constants.clear();
      optimized.push_back(std::move(line));
      continue;
    }

    std::string replaced = replaceOperands(line, constants);
    if (replaced != line) {
      line = std::move(replaced);
      result.changed = true;
    }

    Assignment assignment = parseAssignment(line);
    if (!assignment.valid) {
      if (isTerminator(line)) {
        constants.clear();
      }
      optimized.push_back(std::move(line));
      continue;
    }

    for (std::string &arg : assignment.args) {
      std::string resolved = resolve(arg, constants);
      if (resolved != arg) {
        arg = std::move(resolved);
        result.changed = true;
      }
    }

    if (assignment.args.size() == 2 && isBinaryOp(assignment.op) &&
        isInteger(assignment.args[0]) && isInteger(assignment.args[1]) &&
        !(assignment.op == "div" && assignment.args[1] == "0") &&
        !(assignment.op == "mod" && assignment.args[1] == "0")) {
      long long value =
          evaluateBinary(assignment.op, std::stoll(assignment.args[0]),
                         std::stoll(assignment.args[1]));
      constants[assignment.result] = std::to_string(value);
      if (used_outside.find(assignment.result) == used_outside.end()) {
        result.changed = true;
        continue;
      }
    }

    constants.erase(assignment.result);
    optimized.push_back(formatAssignment(assignment));
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
