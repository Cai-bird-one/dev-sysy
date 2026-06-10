#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>

namespace compiler::ir::opt {

PassResult DeadCodeEliminationPass::run(IrFunction &function) {
  std::map<std::string, int> uses;
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    for (const std::string &name : collectValueNames(line)) {
      if (!assignment.valid || name != assignment.result) {
        ++uses[name];
      }
    }
  }

  PassResult result;
  std::vector<std::string> optimized;
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && uses[assignment.result] == 0 &&
        isSideEffectFree(assignment) && assignment.op != "alloc") {
      result.changed = true;
      continue;
    }
    optimized.push_back(line);
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
