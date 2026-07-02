#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/analysis/liveness_analysis.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <utility>
#include <vector>

namespace compiler::ir::opt {

PassResult DeadCodeEliminationPass::run(IrFunction &function) {
  DenseLivenessInfo liveness = analyzeDenseLiveness(function);
  PassResult result;
  std::vector<std::string> optimized;
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const std::string &line = function.instructions[i];
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && !isLiveOut(liveness, i, assignment.result) &&
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
