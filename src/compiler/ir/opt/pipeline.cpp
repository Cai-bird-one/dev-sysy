#include "compiler/ir/opt/pipeline.h"

#include "compiler/ir/opt/passes/value_passes.h"

#include <memory>

namespace compiler::ir::opt {

PassManager buildPerfPipeline() {
  PassManager passes;
  passes.addModulePass(std::make_unique<UnusedFunctionEliminationPass>());
  passes.addFunctionPass(std::make_unique<ConstantFoldingPass>());
  passes.addFunctionPass(std::make_unique<AlgebraicSimplifyPass>());
  passes.addFunctionPass(std::make_unique<GlobalValuePropagationPass>());
  passes.addFunctionPass(std::make_unique<LocalLoadStoreForwardingPass>());
  passes.addFunctionPass(std::make_unique<LocalDeadStoreEliminationPass>());
  passes.addFunctionPass(
      std::make_unique<CommonSubexpressionEliminationPass>());
  passes.addFunctionPass(std::make_unique<LoopInvariantCodeMotionPass>());
  passes.addFunctionPass(std::make_unique<BranchSimplifyPass>());
  passes.addFunctionPass(std::make_unique<JumpThreadingPass>());
  passes.addFunctionPass(std::make_unique<UnreachableBlockEliminationPass>());
  passes.addFunctionPass(std::make_unique<DeadCodeEliminationPass>());
  passes.addFunctionPass(std::make_unique<BlockCleanupPass>());
  return passes;
}

} // namespace compiler::ir::opt
