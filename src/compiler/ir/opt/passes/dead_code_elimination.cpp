#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>

namespace compiler::ir::opt {

PassResult DeadCodeEliminationPass::run(IrFunction &function) {
  std::map<std::string, int> uses;
  for (const std::string &line : function.instructions) {
    std::vector<std::string> parts = splitWhitespace(line);
    size_t begin = parts.size() >= 3 && parts[1] == "=" ? 3 : 0;
    for (size_t i = begin; i < parts.size(); ++i) {
      if (startsWith(parts[i], "%") || startsWith(parts[i], "@")) {
        ++uses[parts[i]];
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
