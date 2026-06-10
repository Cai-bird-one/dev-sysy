#include "compiler/ir/opt/pipeline.h"

#include "compiler/ir/opt/passes/value_passes.h"

#include <memory>

namespace compiler::ir::opt {

PassManager buildPerfPipeline() {
  PassManager passes;
  passes.addFunctionPass(std::make_unique<ConstantFoldingPass>());
  passes.addFunctionPass(std::make_unique<AlgebraicSimplifyPass>());
  passes.addFunctionPass(std::make_unique<DeadCodeEliminationPass>());
  passes.addFunctionPass(std::make_unique<BlockCleanupPass>());
  return passes;
}

} // namespace compiler::ir::opt
