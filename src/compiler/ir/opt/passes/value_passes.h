#pragma once

#include "compiler/ir/opt/pass.h"

namespace compiler::ir::opt {

class ConstantFoldingPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class AlgebraicSimplifyPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class DeadCodeEliminationPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class BlockCleanupPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

} // namespace compiler::ir::opt
