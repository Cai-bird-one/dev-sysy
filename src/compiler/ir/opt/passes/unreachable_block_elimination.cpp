#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/analysis/control_flow_graph.h"

#include <set>
#include <vector>

namespace compiler::ir::opt {
namespace {

std::set<size_t> reachableBlocks(const ControlFlowGraph &cfg) {
  std::set<size_t> reachable;
  if (cfg.blocks.empty()) {
    return reachable;
  }

  std::vector<size_t> stack = {0};
  while (!stack.empty()) {
    size_t current = stack.back();
    stack.pop_back();
    if (!reachable.insert(current).second) {
      continue;
    }
    for (size_t next : cfg.blocks[current].successors) {
      stack.push_back(next);
    }
  }
  return reachable;
}

} // namespace

PassResult UnreachableBlockEliminationPass::run(IrFunction &function) {
  PassResult result;
  ControlFlowGraph cfg = buildControlFlowGraph(function);
  std::set<size_t> reachable = reachableBlocks(cfg);
  if (reachable.size() == cfg.blocks.size()) {
    return result;
  }

  std::vector<std::string> optimized;
  for (size_t i = 0; i < cfg.blocks.size(); ++i) {
    if (reachable.find(i) == reachable.end()) {
      result.changed = true;
      continue;
    }
    optimized.insert(optimized.end(),
                     function.instructions.begin() + cfg.blocks[i].begin,
                     function.instructions.begin() + cfg.blocks[i].end);
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
