#include "compiler/ir/koopa_generator.h"

#include "compiler/ir/emit/koopa_builders.h"

#include <ostream>

namespace compiler::ir {

std::string KoopaGenerator::generate(
    const compiler::parser::ParseNode &ast) const {
  return generateKoopaProgram(ast);
}

void KoopaGenerator::generate(const compiler::parser::ParseNode &ast,
                              std::ostream &output) const {
  output << generate(ast);
}

} // namespace compiler::ir
