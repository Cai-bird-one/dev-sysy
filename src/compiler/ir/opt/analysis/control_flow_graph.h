#pragma once

#include "compiler/ir/opt/model/ir_module.h"

#include <string>
#include <vector>

namespace compiler::ir::opt {

struct BasicBlock {
  size_t begin = 0;
  size_t end = 0;
  std::string label;
  std::vector<size_t> successors;
  std::vector<size_t> predecessors;
};

struct ControlFlowGraph {
  std::vector<BasicBlock> blocks;
  std::vector<size_t> instruction_blocks;
};

ControlFlowGraph buildControlFlowGraph(const IrFunction &function);

} // namespace compiler::ir::opt
