#include "compiler/ir/emit/expression/constant_expression.h"

#include "compiler/ir/emit/expression/expression_nodes.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <cstdlib>

namespace compiler::ir {

ConstantExpressionEvaluator::ConstantExpressionEvaluator(
    LookupSymbol lookup_symbol)
    : lookup_symbol_(std::move(lookup_symbol)) {}

Value ConstantExpressionEvaluator::evaluate(
    const compiler::parser::ParseNode &node) const {
  if (node.symbol == "INT_CONST") {
    char *end = nullptr;
    long long value = std::strtoll(node.lexeme.c_str(), &end, 0);
    if (end == nullptr || *end != '\0') {
      throw IrError("invalid integer literal: " + node.lexeme);
    }
    return Value{true, value, toOperand(value)};
  }

  if (node.symbol == "LVal") {
    const Symbol &symbol = lookup_symbol_(node.children[0]->lexeme);
    if (symbol.kind != SymbolKind::Constant) {
      throw IrError("global initializer must be constant");
    }
    return Value{true, symbol.const_value, toOperand(symbol.const_value)};
  }

  if (isExpressionWrapper(node.symbol)) {
    if (node.children.size() == 3 && node.children[0]->symbol == "LPAREN") {
      return evaluate(*node.children[1]);
    }
    if (node.children.size() != 1) {
      throw IrError("invalid constant expression node: " + node.symbol);
    }
    return evaluate(*node.children[0]);
  }

  if (node.symbol == "UnaryExp") {
    if (node.children.size() == 1) {
      return evaluate(*node.children[0]);
    }
    if (node.children.size() == 2) {
      const std::string &op = node.children[0]->children[0]->symbol;
      Value value = evaluate(*node.children[1]);
      if (op == "PLUS") {
        return value;
      }
      if (op == "MINUS") {
        long long folded = -value.const_value;
        return Value{true, folded, toOperand(folded)};
      }
      if (op == "NOT") {
        long long folded = value.const_value == 0 ? 1 : 0;
        return Value{true, folded, toOperand(folded)};
      }
    }
  }

  if (isBinaryExpression(node.symbol)) {
    return evaluateBinaryTail(node);
  }

  throw IrError("unsupported global constant expression: " + node.symbol);
}

Value ConstantExpressionEvaluator::evaluateBinaryTail(
    const compiler::parser::ParseNode &node) const {
  if (node.children.size() != 2) {
    throw IrError("invalid constant expression node: " + node.symbol);
  }
  Value lhs = evaluate(*node.children[0]);
  return evaluateTail(*node.children[1], lhs);
}

Value ConstantExpressionEvaluator::evaluateTail(
    const compiler::parser::ParseNode &node, Value lhs) const {
  if (node.children.empty()) {
    return lhs;
  }
  if (node.children.size() != 3) {
    throw IrError("invalid constant expression tail node: " + node.symbol);
  }
  Value rhs = evaluate(*node.children[1]);
  long long value = foldBinary(node.children[0]->symbol, lhs.const_value,
                               rhs.const_value);
  return evaluateTail(*node.children[2], Value{true, value, toOperand(value)});
}

} // namespace compiler::ir
