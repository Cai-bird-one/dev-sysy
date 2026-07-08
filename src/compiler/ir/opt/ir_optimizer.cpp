#include "compiler/ir/opt/ir_optimizer.h"

#include "compiler/ir/opt/emit/koopa_ir_writer.h"
#include "compiler/ir/opt/parse/koopa_ir_parser.h"
#include "compiler/ir/opt/pipeline.h"

namespace compiler::ir::opt {
namespace {

constexpr int kMaxOptimizationIterations = 128;

} // namespace

std::string IrOptimizer::optimize(const std::string &koopa_ir) const {
  IrModule module = KoopaIrParser().parse(koopa_ir);

  for (int i = 0; i < kMaxOptimizationIterations; ++i) {
    PassManager passes = buildPerfPipeline();
    if (!passes.run(module).changed) {
      break;
    }
  }

  return KoopaIrWriter().write(module);
}

} // namespace compiler::ir::opt
