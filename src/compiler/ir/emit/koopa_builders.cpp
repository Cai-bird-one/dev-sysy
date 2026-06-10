#include "compiler/ir/emit/koopa_builders.h"

#include "compiler/ir/emit/program/program_builder.h"

namespace compiler::ir {

std::string generateKoopaProgram(const compiler::parser::ParseNode &ast) {
  return ProgramBuilder().generate(ast);
}

} // namespace compiler::ir
