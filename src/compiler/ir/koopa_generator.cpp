#include "compiler/ir/koopa_generator.h"

#include <cstdlib>
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
  const compiler::parser::ParseNode *exp = findFirst(ast, "Exp");
  if (exp == nullptr) {
    throw IrError("cannot find return expression in AST");
  }
  return std::to_string(evaluateExpression(*exp));
}

long long KoopaGenerator::evaluateExpression(
    const compiler::parser::ParseNode &node) const {
  if (node.symbol == "INT_CONST") {
    char *end = nullptr;
    long long value = std::strtoll(node.lexeme.c_str(), &end, 0);
    if (end == nullptr || *end != '\0') {
      throw IrError("invalid integer literal: " + node.lexeme);
    }
    return value;
  }

  if (node.symbol == "Number") {
    if (node.children.size() != 1) {
      throw IrError("invalid Number node");
    }
    return evaluateExpression(*node.children[0]);
  }

  if (node.symbol == "Exp") {
    if (node.children.size() != 1) {
      throw IrError("invalid Exp node");
    }
    return evaluateExpression(*node.children[0]);
  }

  if (node.symbol == "UnaryExp") {
    if (node.children.size() == 1) {
      return evaluateExpression(*node.children[0]);
    }
    if (node.children.size() == 2) {
      const std::string &op = node.children[0]->children[0]->symbol;
      long long value = evaluateExpression(*node.children[1]);
      if (op == "PLUS") {
        return value;
      }
      if (op == "MINUS") {
        return -value;
      }
      if (op == "NOT") {
        return value == 0 ? 1 : 0;
      }
    }
    throw IrError("invalid UnaryExp node");
  }

  if (node.symbol == "PrimaryExp") {
    if (node.children.size() == 1) {
      return evaluateExpression(*node.children[0]);
    }
    if (node.children.size() == 3) {
      return evaluateExpression(*node.children[1]);
    }
    throw IrError("invalid PrimaryExp node");
  }

  throw IrError("unsupported expression node: " + node.symbol);
}

} // namespace compiler::ir
