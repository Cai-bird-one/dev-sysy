#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <set>
#include <utility>

namespace compiler::ir::opt {
namespace {

std::set<std::string> collectUsedValues(const IrFunction &function) {
  std::set<std::string> used;
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid) {
      for (const std::string &arg : assignment.args) {
        if (isValueName(arg)) {
          used.insert(arg);
        }
      }
      continue;
    }
    for (const std::string &value : collectValueNames(line)) {
      used.insert(value);
    }
  }
  return used;
}

} // namespace

PassResult UnusedAllocEliminationPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> used = collectUsedValues(function);
  std::vector<std::string> optimized;

  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && assignment.op == "alloc" &&
        used.find(assignment.result) == used.end()) {
      result.changed = true;
      continue;
    }
    optimized.push_back(line);
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
