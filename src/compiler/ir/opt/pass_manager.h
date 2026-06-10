#pragma once

#include "compiler/ir/opt/pass.h"

#include <memory>
#include <vector>

namespace compiler::ir::opt {

class PassManager {
public:
  void addModulePass(std::unique_ptr<ModulePass> pass);
  void addFunctionPass(std::unique_ptr<FunctionPass> pass);
  PassResult run(IrModule &module) const;

private:
  std::vector<std::unique_ptr<ModulePass>> module_passes_;
  std::vector<std::unique_ptr<FunctionPass>> function_passes_;
};

} // namespace compiler::ir::opt
