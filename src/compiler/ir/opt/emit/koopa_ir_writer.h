#pragma once

#include "compiler/ir/opt/model/ir_module.h"

#include <string>

namespace compiler::ir::opt {

class KoopaIrWriter {
public:
  std::string write(const IrModule &module) const;
};

} // namespace compiler::ir::opt
