#include "compiler/ir/emit/expression/runtime_expression_translator.h"

#include "compiler/ir/koopa_generator.h"

#include <cstdlib>

namespace compiler::ir {

Value RuntimeExpressionTranslator::emitNumber(
    const compiler::parser::ParseNode &node) const {
  const compiler::parser::ParseNode &number =
      node.symbol == "INT_CONST" ? node : *node.children[0];
  char *end = nullptr;
  long long value = std::strtoll(number.lexeme.c_str(), &end, 0);
  if (end == nullptr || *end != '\0') {
    throw IrError("invalid integer literal: " + number.lexeme);
  }
  return context_.makeConstant(value);
}

Value RuntimeExpressionTranslator::emitParenthesizedExpression(
    const compiler::parser::ParseNode &node) const {
  if (node.children.size() != 3 || node.children[0]->symbol != "LPAREN") {
    throw IrError("invalid parenthesized expression node");
  }
  return translate(*node.children[1]);
}

Value RuntimeExpressionTranslator::emitLValExpression(
    const compiler::parser::ParseNode &node) const {
  return context_.emitLVal(node);
}

Value RuntimeExpressionTranslator::emitCallExpression(
    const compiler::parser::ParseNode &node) const {
  return context_.emitCall(node);
}

Value RuntimeExpressionTranslator::translateSingleChild(
    const compiler::parser::ParseNode &node) const {
  if (node.children.size() != 1) {
    throw IrError("invalid expression wrapper node: " + node.symbol);
  }
  return translate(*node.children[0]);
}

Value RuntimeExpressionTranslator::emitUnaryExpression(
    const compiler::parser::ParseNode &node) const {
  if (node.children.size() == 2) {
    const std::string &op = node.children[0]->children[0]->symbol;
    Value value = translate(*node.children[1]);
    if (op == "PLUS") {
      return value;
    }
    if (value.constant) {
      if (op == "MINUS") {
        return context_.makeConstant(-value.const_value);
      }
      if (op == "NOT") {
        return context_.makeConstant(value.const_value == 0 ? 1 : 0);
      }
    }
    if (op == "MINUS") {
      return context_.emitBinary("MINUS", context_.makeConstant(0), value);
    }
    if (op == "NOT") {
      return context_.emitBinary("EQ", value, context_.makeConstant(0));
    }
  }
  throw IrError("unsupported UnaryExp node");
}

Value RuntimeExpressionTranslator::emitBinaryExpression(
    const compiler::parser::ParseNode &node,
    const std::string &tail_symbol) const {
  if (node.children.size() != 2 || node.children[1]->symbol != tail_symbol) {
    throw IrError("invalid " + node.symbol + " node");
  }
  Value lhs = translate(*node.children[0]);
  if (tail_symbol == "LAndExpTail") {
    if (node.children[1]->children.empty()) {
      return lhs;
    }
    return context_.emitShortCircuitTail(*node.children[1], lhs, false);
  }
  if (tail_symbol == "LOrExpTail") {
    if (node.children[1]->children.empty()) {
      return lhs;
    }
    return context_.emitShortCircuitTail(*node.children[1], lhs, true);
  }
  return translateTail(*node.children[1], lhs);
}

Value RuntimeExpressionTranslator::translateTail(
    const compiler::parser::ParseNode &node, Value lhs) const {
  if (node.children.empty()) {
    return lhs;
  }
  if (node.children.size() != 3) {
    throw IrError("invalid expression tail node: " + node.symbol);
  }

  const std::string &op = node.children[0]->symbol;
  Value rhs = translate(*node.children[1]);
  Value combined = context_.emitBinary(op, lhs, rhs);
  return translateTail(*node.children[2], combined);
}

} // namespace compiler::ir
