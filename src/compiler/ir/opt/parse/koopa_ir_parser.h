#pragma once

#include "compiler/ir/opt/model/ir_module.h"

#include <string>

namespace compiler::ir::opt {

class KoopaIrParser {
public:
  IrModule parse(const std::string &koopa_ir) const;
};

} // namespace compiler::ir::opt
