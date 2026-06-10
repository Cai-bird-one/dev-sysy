#include "compiler/ir/opt/parse/koopa_ir_parser.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <sstream>
#include <utility>

namespace compiler::ir::opt {

IrModule KoopaIrParser::parse(const std::string &koopa_ir) const {
  IrModule module;
  std::istringstream input(koopa_ir);
  std::string line;
  bool in_function = false;
  IrFunction current;

  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (startsWith(line, "fun @")) {
      if (in_function) {
        throw IrError("nested function in Koopa IR optimizer");
      }
      current = IrFunction{};
      current.header = line;
      in_function = true;
      continue;
    }

    if (line == "}") {
      if (!in_function) {
        throw IrError("unexpected function end in Koopa IR optimizer");
      }
      module.functions.push_back(std::move(current));
      in_function = false;
      continue;
    }

    if (in_function) {
      current.instructions.push_back(line);
    } else {
      module.globals.push_back(line);
    }
  }

  if (in_function) {
    throw IrError("unterminated function in Koopa IR optimizer");
  }
  return module;
}

} // namespace compiler::ir::opt
