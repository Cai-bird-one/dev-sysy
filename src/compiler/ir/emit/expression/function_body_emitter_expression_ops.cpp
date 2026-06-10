#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

namespace compiler::ir {

Value FunctionBodyEmitter::emitPointerArgument(
    const compiler::parser::ParseNode &node, const std::string &parameter_type) {
  const compiler::parser::ParseNode *lval = unwrapArrayArgument(node);
  if (lval == nullptr || lval->children.empty() ||
      lval->children[0]->symbol != "IDENT") {
    throw IrError("array argument must be an lvalue");
  }
  const Symbol &symbol = lookup(lval->children[0]->lexeme);
  if (symbol.kind != SymbolKind::Variable ||
      (symbol.dimensions.empty() && !symbol.pointer_parameter)) {
    throw IrError("array argument must be an array: " +
                  lval->children[0]->lexeme);
  }

  std::vector<Value> indices = collectLValIndices(*lval);
  std::vector<long long> expected_dimensions =
      parsePointerTypeDimensions(parameter_type);
  std::string pointer =
      emitDecayedArrayPointer(symbol, indices, expected_dimensions);
  return Value{false, 0, pointer};
}

Value FunctionBodyEmitter::emitShortCircuitTail(
    const compiler::parser::ParseNode &node, Value lhs, bool is_or) {
  if (node.children.empty()) {
    return emitBoolean(lhs);
  }
  if (node.children.size() != 3) {
    throw IrError("invalid logical expression tail node: " + node.symbol);
  }

  if (lhs.constant) {
    bool lhs_true = lhs.const_value != 0;
    if ((is_or && lhs_true) || (!is_or && !lhs_true)) {
      return makeConstant(lhs_true ? 1 : 0);
    }
    Value rhs = emitExpression(*node.children[1]);
    return emitShortCircuitTail(*node.children[2], rhs, is_or);
  }

  Value bool_lhs = emitBoolean(lhs);
  std::string result_pointer = newTemp();
  emitLocalAlloc(result_pointer + " = alloc i32");
  emit("store " + std::string(is_or ? "1" : "0") + ", " + result_pointer);

  std::string eval_label = newLabel(is_or ? "or_rhs" : "and_rhs");
  std::string end_label = newLabel(is_or ? "or_end" : "and_end");
  if (is_or) {
    emit("br " + bool_lhs.operand + ", " + end_label + ", " + eval_label);
  } else {
    emit("br " + bool_lhs.operand + ", " + eval_label + ", " + end_label);
  }

  emitLabel(eval_label);
  Value rhs = emitExpression(*node.children[1]);
  Value rhs_value = emitShortCircuitTail(*node.children[2], rhs, is_or);
  emit("store " + rhs_value.operand + ", " + result_pointer);
  emit("jump " + end_label);

  emitLabel(end_label);
  std::string loaded = newTemp();
  emit(loaded + " = load " + result_pointer);
  return Value{false, 0, loaded};
}

Value FunctionBodyEmitter::emitBinary(const std::string &op, Value lhs, Value rhs) {
  if (lhs.constant && rhs.constant) {
    return makeConstant(foldBinary(op, lhs.const_value, rhs.const_value));
  }
  if (op == "AND" || op == "OR") {
    lhs = emitBoolean(lhs);
    rhs = emitBoolean(rhs);
  }
  std::string result = newTemp();
  emit(result + " = " + koopaOp(op) + " " + lhs.operand + ", " + rhs.operand);
  return Value{false, 0, result};
}

Value FunctionBodyEmitter::emitBoolean(Value value) {
  if (value.constant) {
    return makeConstant(value.const_value != 0 ? 1 : 0);
  }
  std::string result = newTemp();
  emit(result + " = ne " + value.operand + ", 0");
  return Value{false, 0, result};
}

} // namespace compiler::ir
