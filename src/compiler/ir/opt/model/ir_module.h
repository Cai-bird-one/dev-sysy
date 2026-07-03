#pragma once

#include "compiler/ir/tensor/koopa_tensor_ir.h"

#include <string>
#include <vector>

namespace compiler::ir::opt {

struct IrFunction {
  std::string header;
  std::vector<std::string> instructions;
};

struct IrModule {
  KoopaTensorInfo tensor;
  std::vector<std::string> globals;
  std::vector<IrFunction> functions;
};

} // namespace compiler::ir::opt
