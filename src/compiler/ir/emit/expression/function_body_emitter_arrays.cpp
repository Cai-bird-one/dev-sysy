#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/ast/parse_node_utils.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

namespace compiler::ir {

std::string FunctionBodyEmitter::emitDecayedArrayPointer(
    const Symbol &symbol, const std::vector<Value> &indices,
    const std::vector<long long> &expected_dimensions) {
  size_t consumed_dimensions = 0;
  std::string pointer;
  if (symbol.pointer_parameter) {
    if (indices.empty()) {
      pointer = symbol.pointer;
    } else {
      pointer = emitArrayElementPointer(symbol.pointer, symbol.dimensions,
                                        indices, true);
      consumed_dimensions = indices.size() - 1;
    }
  } else {
    std::vector<Value> actual_indices = indices;
    if (actual_indices.empty()) {
      actual_indices.push_back(makeConstant(0));
    }
    pointer = emitArrayElementPointer(symbol.pointer, symbol.dimensions,
                                      actual_indices, false);
    consumed_dimensions = actual_indices.size();
  }

  if (consumed_dimensions > symbol.dimensions.size()) {
    throw IrError("too many indices for array argument");
  }
  std::vector<long long> remaining(symbol.dimensions.begin() +
                                       consumed_dimensions,
                                   symbol.dimensions.end());
  while (remaining.size() > expected_dimensions.size()) {
    std::string next = newTemp();
    emit(next + " = getelemptr " + pointer + ", 0");
    pointer = next;
    remaining.erase(remaining.begin());
  }
  if (remaining != expected_dimensions) {
    throw IrError("array argument type mismatch");
  }
  return pointer;
}

std::vector<Value>
FunctionBodyEmitter::collectLValIndices(const compiler::parser::ParseNode &lval) {
  std::vector<Value> indices;
  if (lval.children.size() < 2) {
    return indices;
  }
  const compiler::parser::ParseNode *dims = lval.children[1].get();
  while (dims != nullptr && !dims->children.empty()) {
    if (dims->children.size() != 4) {
      throw IrError("invalid LVal array dimensions");
    }
    indices.push_back(emitExpression(*dims->children[1]));
    dims = dims->children[3].get();
  }
  return indices;
}

std::string FunctionBodyEmitter::emitArrayElementPointer(
    const std::string &base_pointer, const std::vector<long long> &dimensions,
    long long linear_index) {
  std::vector<Value> indices;
  for (size_t i = 0; i < dimensions.size(); ++i) {
    long long stride = elementCount(dimensions, i + 1);
    indices.push_back(makeConstant(linear_index / stride));
    linear_index %= stride;
  }
  return emitArrayElementPointer(base_pointer, dimensions, indices, false);
}

std::string FunctionBodyEmitter::emitArrayElementPointer(
    const std::string &base_pointer, const std::vector<long long> &dimensions,
    const std::vector<Value> &indices, bool first_getptr) {
  size_t max_indices = dimensions.size() + (first_getptr ? 1 : 0);
  if (indices.empty() || indices.size() > max_indices) {
    throw IrError("invalid array index count");
  }
  std::string pointer = base_pointer;
  for (size_t i = 0; i < indices.size(); ++i) {
    std::string next = newTemp();
    emit(next + " = " + std::string(first_getptr && i == 0 ? "getptr "
                                                           : "getelemptr ") +
         pointer + ", " + indices[i].operand);
    pointer = next;
  }
  return pointer;
}

std::vector<long long> FunctionBodyEmitter::collectArrayDimensions(
    const compiler::parser::ParseNode &node) {
  std::vector<long long> dimensions;
  const compiler::parser::ParseNode *current = &node;
  while (current != nullptr && !current->children.empty()) {
    if (current->children.size() != 4) {
      throw IrError("invalid array dimensions");
    }
    Value value = emitExpression(*current->children[1]);
    long long dimension = expectConstant(value, "array dimension");
    if (dimension <= 0) {
      throw IrError("array dimension must be positive");
    }
    dimensions.push_back(dimension);
    current = current->children[3].get();
  }
  return dimensions;
}

} // namespace compiler::ir
