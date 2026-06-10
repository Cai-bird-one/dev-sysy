#pragma once

#include <string>

namespace compiler::ir::opt {

class IrOptimizer {
public:
  std::string optimize(const std::string &koopa_ir) const;
};

} // namespace compiler::ir::opt
