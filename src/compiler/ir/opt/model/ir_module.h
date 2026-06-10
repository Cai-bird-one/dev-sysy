#pragma once

#include <string>
#include <vector>

namespace compiler::ir::opt {

struct IrFunction {
  std::string header;
  std::vector<std::string> instructions;
};

struct IrModule {
  std::vector<std::string> globals;
  std::vector<IrFunction> functions;
};

} // namespace compiler::ir::opt
