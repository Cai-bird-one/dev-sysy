#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/tensor/koopa_tensor_optimize.h"

namespace compiler::ir::opt {

PassResult TensorDialectOptimizationPass::run(IrModule &module) {
  std::string before = formatTensorInfo(module.tensor);
  module.tensor = optimizeTensorInfo(module.tensor);
  return {formatTensorInfo(module.tensor) != before};
}

} // namespace compiler::ir::opt
