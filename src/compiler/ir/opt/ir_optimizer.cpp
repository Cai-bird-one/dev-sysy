#include "compiler/ir/opt/ir_optimizer.h"

#include "compiler/ir/koopa_tensor_extension.h"
#include "compiler/ir/opt/emit/koopa_ir_writer.h"
#include "compiler/ir/opt/parse/koopa_ir_parser.h"
#include "compiler/ir/opt/pipeline.h"

namespace compiler::ir::opt {

std::string IrOptimizer::optimize(const std::string &koopa_ir) const {
  compiler::ir::KoopaTensorExtension tensor_extension;
  compiler::ir::KoopaTensorInfo tensor_info =
      tensor_extension.optimize(koopa_ir);

  IrModule module = KoopaIrParser().parse(koopa_ir);

  for (int i = 0; i < 8; ++i) {
    PassManager passes = buildPerfPipeline();
    if (!passes.run(module).changed) {
      break;
    }
  }

  std::string tensor_annotations = tensor_extension.format(tensor_info);
  std::string optimized_ir = KoopaIrWriter().write(module);
  if (tensor_annotations.empty()) {
    return optimized_ir;
  }
  return tensor_annotations + "\n" + optimized_ir;
}

} // namespace compiler::ir::opt
