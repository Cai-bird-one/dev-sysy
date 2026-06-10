#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/koopa_generator.h"

namespace compiler::ir {

Value FunctionBodyEmitter::emitLVal(const compiler::parser::ParseNode &node) {
  if (node.children.empty() || node.children[0]->symbol != "IDENT") {
    throw IrError("invalid LVal node");
  }

  const Symbol &symbol = lookup(node.children[0]->lexeme);
  if (symbol.kind == SymbolKind::Constant) {
    return makeConstant(symbol.const_value);
  }
  if (!symbol.dimensions.empty() || symbol.pointer_parameter) {
    std::vector<Value> indices = collectLValIndices(node);
    std::string pointer = emitArrayAccessPointer(symbol, indices);
    size_t required_indices = symbol.dimensions.size() +
                              (symbol.pointer_parameter ? 1 : 0);
    if (indices.size() < required_indices) {
      return Value{false, 0, pointer};
    }
    std::string loaded = newTemp();
    emit(loaded + " = load " + pointer);
    return Value{false, 0, loaded};
  }

  std::string loaded = newTemp();
  emit(loaded + " = load " + symbol.pointer);
  return Value{false, 0, loaded};
}

std::string
FunctionBodyEmitter::lookupVariablePointer(const compiler::parser::ParseNode &lval) {
  if (lval.children.empty() || lval.children[0]->symbol != "IDENT") {
    throw IrError("invalid assignment LVal");
  }

  const Symbol &symbol = lookup(lval.children[0]->lexeme);
  if (symbol.kind != SymbolKind::Variable || !symbol.assignable) {
    throw IrError("cannot assign to constant: " + lval.children[0]->lexeme);
  }
  if (!symbol.dimensions.empty() || symbol.pointer_parameter) {
    std::vector<Value> indices = collectLValIndices(lval);
    size_t required_indices = symbol.dimensions.size() +
                              (symbol.pointer_parameter ? 1 : 0);
    if (indices.size() != required_indices) {
      throw IrError("array assignment must index every dimension: " +
                    lval.children[0]->lexeme);
    }
    return emitArrayAccessPointer(symbol, indices);
  }
  return symbol.pointer;
}

std::string
FunctionBodyEmitter::emitArrayAccessPointer(const Symbol &symbol,
                                        const std::vector<Value> &indices) {
  if (indices.empty()) {
    if (symbol.pointer_parameter) {
      return symbol.pointer;
    }
    return emitArrayElementPointer(symbol.pointer, symbol.dimensions,
                                   std::vector<Value>{makeConstant(0)}, false);
  }
  return emitArrayElementPointer(symbol.pointer, symbol.dimensions, indices,
                                 symbol.pointer_parameter);
}

} // namespace compiler::ir
