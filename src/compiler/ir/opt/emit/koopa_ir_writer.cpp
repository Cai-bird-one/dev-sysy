#include "compiler/ir/opt/emit/koopa_ir_writer.h"

#include <sstream>

namespace compiler::ir::opt {

std::string KoopaIrWriter::write(const IrModule &module) const {
  std::ostringstream output;
  std::string tensor_dialect = formatTensorInfo(module.tensor);
  if (!tensor_dialect.empty()) {
    output << tensor_dialect << '\n';
  }
  for (const std::string &global : module.globals) {
    output << global << '\n';
  }
  if (!module.globals.empty() && !module.functions.empty()) {
    output << '\n';
  }

  for (size_t i = 0; i < module.functions.size(); ++i) {
    const IrFunction &function = module.functions[i];
    output << function.header << '\n';
    for (const std::string &instruction : function.instructions) {
      if (!instruction.empty() && instruction.back() == ':') {
        output << instruction << '\n';
      } else {
        output << "  " << instruction << '\n';
      }
    }
    output << "}\n";
    if (i + 1 != module.functions.size()) {
      output << '\n';
    }
  }
  return output.str();
}

} // namespace compiler::ir::opt
