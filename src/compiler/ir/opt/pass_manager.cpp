#include "compiler/ir/opt/pass_manager.h"

#include <utility>

namespace compiler::ir::opt {

void PassManager::addModulePass(std::unique_ptr<ModulePass> pass) {
  module_passes_.push_back(std::move(pass));
}

void PassManager::addFunctionPass(std::unique_ptr<FunctionPass> pass) {
  function_passes_.push_back(std::move(pass));
}

PassResult PassManager::run(IrModule &module) const {
  PassResult result;
  for (const auto &pass : module_passes_) {
    result.changed = pass->run(module).changed || result.changed;
  }
  for (IrFunction &function : module.functions) {
    for (const auto &pass : function_passes_) {
      result.changed = pass->run(function).changed || result.changed;
    }
  }
  return result;
}

} // namespace compiler::ir::opt
