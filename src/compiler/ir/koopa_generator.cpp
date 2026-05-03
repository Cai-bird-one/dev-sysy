#include "compiler/ir/koopa_generator.h"

#include <cstdlib>
#include <map>
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
  std::map<std::string, long long> symbols;
  const compiler::parser::ParseNode *return_exp = nullptr;
  const compiler::parser::ParseNode *block = findFirst(ast, "Block");
  if (block == nullptr) {
    throw IrError("cannot find function block in AST");
  }
  collectBlockItems(*block, symbols, return_exp);
  if (return_exp == nullptr) {
    throw IrError("cannot find return expression in AST");
  }
  return std::to_string(evaluateExpression(*return_exp, symbols));
}

long long KoopaGenerator::evaluateExpression(
    const compiler::parser::ParseNode &node,
    const std::map<std::string, long long> &symbols) const {
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
    return evaluateExpression(*node.children[0], symbols);
  }

  if (node.symbol == "LVal") {
    if (node.children.size() != 1 || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid LVal node");
    }
    auto found = symbols.find(node.children[0]->lexeme);
    if (found == symbols.end()) {
      throw IrError("unknown identifier: " + node.children[0]->lexeme);
    }
    return found->second;
  }

  if (node.symbol == "Exp" || node.symbol == "ConstExp" ||
      node.symbol == "ConstInitVal" || node.symbol == "InitVal") {
    if (node.children.size() != 1) {
      throw IrError("invalid expression wrapper node: " + node.symbol);
    }
    return evaluateExpression(*node.children[0], symbols);
  }

  if (node.symbol == "UnaryExp") {
    if (node.children.size() == 1) {
      return evaluateExpression(*node.children[0], symbols);
    }
    if (node.children.size() == 2) {
      const std::string &op = node.children[0]->children[0]->symbol;
      long long value = evaluateExpression(*node.children[1], symbols);
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
      return evaluateExpression(*node.children[0], symbols);
    }
    if (node.children.size() == 3) {
      return evaluateExpression(*node.children[1], symbols);
    }
    throw IrError("invalid PrimaryExp node");
  }

  throw IrError("unsupported expression node: " + node.symbol);
}

void KoopaGenerator::collectBlockItems(
    const compiler::parser::ParseNode &node,
    std::map<std::string, long long> &symbols,
    const compiler::parser::ParseNode *&return_exp) const {
  if (node.symbol == "Decl") {
    collectDeclaration(node, symbols);
    return;
  }

  if (node.symbol == "Stmt") {
    for (const auto &child : node.children) {
      if (child->symbol == "Exp") {
        return_exp = child.get();
        return;
      }
    }
  }

  for (const auto &child : node.children) {
    collectBlockItems(*child, symbols, return_exp);
  }
}

void KoopaGenerator::collectDeclaration(
    const compiler::parser::ParseNode &node,
    std::map<std::string, long long> &symbols) const {
  if (node.symbol == "ConstDef") {
    if (node.children.size() != 3 || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid ConstDef node");
    }
    symbols[node.children[0]->lexeme] =
        evaluateExpression(*node.children[2], symbols);
    return;
  }

  if (node.symbol == "VarDef") {
    if (node.children.empty() || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid VarDef node");
    }
    long long value = 0;
    if (node.children.size() == 2 && !node.children[1]->children.empty()) {
      value = evaluateExpression(*node.children[1]->children[1], symbols);
    }
    symbols[node.children[0]->lexeme] = value;
    return;
  }

  for (const auto &child : node.children) {
    collectDeclaration(*child, symbols);
  }
}

} // namespace compiler::ir
