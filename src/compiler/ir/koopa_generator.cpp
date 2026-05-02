#include "compiler/ir/koopa_generator.h"

#include <ostream>
#include <sstream>

namespace compiler::ir {
namespace {

const compiler::parser::ParseNode *
findFirst(const compiler::parser::ParseNode &node, const std::string &symbol) {
  if (node.symbol == symbol) {
    return &node;
  }
  for (const auto &child : node.children) {
    const compiler::parser::ParseNode *found = findFirst(*child, symbol);
    if (found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

} // namespace

std::string KoopaGenerator::generate(
    const compiler::parser::ParseNode &ast) const {
  std::ostringstream output;
  generate(ast, output);
  return output.str();
}

void KoopaGenerator::generate(const compiler::parser::ParseNode &ast,
                              std::ostream &output) const {
  std::string function_name = findFunctionName(ast);
  std::string return_value = findReturnValue(ast);

  output << "fun @" << function_name << "(): i32 {\n"
         << "%entry:\n"
         << "  ret " << return_value << "\n"
         << "}\n";
}

std::string KoopaGenerator::findFunctionName(
    const compiler::parser::ParseNode &ast) const {
  const compiler::parser::ParseNode *ident = findFirst(ast, "IDENT");
  if (ident == nullptr || ident->lexeme.empty()) {
    throw IrError("cannot find function name in AST");
  }
  return ident->lexeme;
}

std::string KoopaGenerator::findReturnValue(
    const compiler::parser::ParseNode &ast) const {
  const compiler::parser::ParseNode *int_const = findFirst(ast, "INT_CONST");
  if (int_const == nullptr || int_const->lexeme.empty()) {
    throw IrError("cannot find return value in AST");
  }
  return int_const->lexeme;
}

} // namespace compiler::ir
