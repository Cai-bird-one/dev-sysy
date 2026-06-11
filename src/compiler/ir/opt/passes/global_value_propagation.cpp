#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/analysis/control_flow_graph.h"
#include "compiler/ir/opt/passes/global_value_facts.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <utility>
#include <vector>

namespace compiler::ir::opt {

PassResult GlobalValuePropagationPass::run(IrFunction &function) {
  PassResult result;
  ControlFlowGraph cfg = buildControlFlowGraph(function);
  std::vector<ValueFacts> block_in =
      computeBlockEntryValueFacts(function, cfg);
  std::vector<std::string> optimized = function.instructions;

  for (size_t block_index = 0; block_index < cfg.blocks.size();
       ++block_index) {
    ValueFacts facts = block_in[block_index];
    const BasicBlock &block = cfg.blocks[block_index];
    for (size_t i = block.begin; i < block.end; ++i) {
      std::string line = optimized[i];
      if (isLabel(line)) {
        continue;
      }

      Assignment assignment = parseAssignment(line);
      if (assignment.valid) {
        Assignment replaced =
            assignmentWithResolvedValueFacts(assignment, facts);
        std::string formatted = formatAssignment(replaced);
        if (formatted != line) {
          optimized[i] = formatted;
          result.changed = true;
        }
        updateValueFactsForAssignment(replaced, facts);
        continue;
      }

      std::string replaced =
          replaceOperands(line, valueFactReplacements(facts));
      if (replaced != line) {
        optimized[i] = std::move(replaced);
        result.changed = true;
      }
    }
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
