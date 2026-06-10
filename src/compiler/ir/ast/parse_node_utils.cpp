#include "compiler/ir/ast/parse_node_utils.h"

#include "compiler/ir/koopa_generator.h"

namespace compiler::ir {

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

const compiler::parser::ParseNode *
findDirectChild(const compiler::parser::ParseNode &node,
                const std::string &symbol) {
  for (const auto &child : node.children) {
    if (child->symbol == symbol) {
      return child.get();
    }
  }
  return nullptr;
}

void collectNodes(const compiler::parser::ParseNode &node,
                  const std::string &symbol,
                  std::vector<const compiler::parser::ParseNode *> &out) {
  if (node.symbol == symbol) {
    out.push_back(&node);
  }
  for (const auto &child : node.children) {
    collectNodes(*child, symbol, out);
  }
}

bool hasNonEmptyChild(const compiler::parser::ParseNode &node,
                      const std::string &symbol) {
  const compiler::parser::ParseNode *child = findDirectChild(node, symbol);
  return child != nullptr && !child->children.empty();
}

std::string findFunctionName(const compiler::parser::ParseNode &function) {
  const compiler::parser::ParseNode *ident = findDirectChild(function, "IDENT");
  if (ident == nullptr || ident->lexeme.empty()) {
    ident = findFirst(function, "IDENT");
  }
  if (ident == nullptr || ident->lexeme.empty()) {
    throw IrError("cannot find function name in AST");
  }
  return ident->lexeme;
}

std::string findFunctionReturnType(const compiler::parser::ParseNode &function) {
  const compiler::parser::ParseNode *func_type =
      findDirectChild(function, "FuncType");
  if (func_type != nullptr && !func_type->children.empty() &&
      func_type->children[0]->symbol == "VOID") {
    return "void";
  }
  return "i32";
}

} // namespace compiler::ir
