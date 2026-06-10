#pragma once

#include "compiler/ir/opt/model/ir_module.h"

namespace compiler::ir::opt {

struct PassResult {
  bool changed = false;
};

class FunctionPass {
public:
  virtual ~FunctionPass() = default;
  virtual PassResult run(IrFunction &function) = 0;
};

class ModulePass {
public:
  virtual ~ModulePass() = default;
  virtual PassResult run(IrModule &module) = 0;
};

} // namespace compiler::ir::opt
