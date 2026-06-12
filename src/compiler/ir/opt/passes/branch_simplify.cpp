#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <utility>

namespace compiler::ir::opt {

PassResult BranchSimplifyPass::run(IrFunction &function) {
  PassResult result;
  std::vector<std::string> optimized;

  for (const std::string &line : function.instructions) {
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.size() == 4 && parts[0] == "br" && isInteger(parts[1])) {
      const std::string &target = parts[1] == "0" ? parts[3] : parts[2];
      optimized.push_back("jump " + target);
      result.changed = true;
      continue;
    }
    if (parts.size() == 4 && parts[0] == "br" && parts[2] == parts[3]) {
      optimized.push_back("jump " + parts[2]);
      result.changed = true;
      continue;
    }
    optimized.push_back(line);
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
