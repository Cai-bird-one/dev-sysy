#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

namespace compiler::ir::opt {

PassResult BlockCleanupPass::run(IrFunction &function) {
  PassResult result;
  std::vector<std::string> optimized;
  bool unreachable_until_label = false;

  for (const std::string &line : function.instructions) {
    if (isLabel(line)) {
      unreachable_until_label = false;
      optimized.push_back(line);
      continue;
    }
    if (unreachable_until_label) {
      result.changed = true;
      continue;
    }
    optimized.push_back(line);
    if (isTerminator(line)) {
      unreachable_until_label = true;
    }
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
