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

class GlobalValuePropagationPass : public FunctionPass {
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

class BranchSimplifyPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class JumpThreadingPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class UnreachableBlockEliminationPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class CommonSubexpressionEliminationPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class LocalLoadStoreForwardingPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

class LocalDeadStoreEliminationPass : public FunctionPass {
public:
  PassResult run(IrFunction &function) override;
};

} // namespace compiler::ir::opt
