#include "compiler/ir/emit/program/program_builder.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <sstream>

namespace compiler::ir {

std::vector<long long> ProgramBuilder::collectGlobalArrayDimensions(
    const compiler::parser::ParseNode &node) {
  std::vector<long long> dimensions;
  const compiler::parser::ParseNode *current = &node;
  while (current != nullptr && !current->children.empty()) {
    if (current->children.size() != 4) {
      throw IrError("invalid global array dimensions");
    }
    long long dimension =
        expectConstant(emitGlobalExpression(*current->children[1]),
                       "global array dimension");
    if (dimension <= 0) {
      throw IrError("array dimension must be positive");
    }
    dimensions.push_back(dimension);
    current = current->children[3].get();
  }
  return dimensions;
}

void ProgramBuilder::collectInitializerChildren(
    const compiler::parser::ParseNode &node,
    std::vector<const compiler::parser::ParseNode *> &out) {
  if (node.children.empty()) {
    return;
  }
  if (node.symbol == "InitValListOpt" ||
      node.symbol == "ConstInitValListOpt") {
    out.push_back(node.children[0].get());
    collectInitializerChildren(*node.children[1], out);
    return;
  }
  if (node.symbol == "InitValListTail" ||
      node.symbol == "ConstInitValListTail") {
    if (node.children.empty() || node.children.size() == 1) {
      return;
    }
    out.push_back(node.children[1].get());
    collectInitializerChildren(*node.children[2], out);
    return;
  }
  throw IrError("invalid global array initializer list: " + node.symbol);
}

bool ProgramBuilder::isInitializerList(
    const compiler::parser::ParseNode &node) const {
  return !node.children.empty() && node.children[0]->symbol == "LBRACE";
}

long long ProgramBuilder::fillGlobalInitializer(
    const compiler::parser::ParseNode &node,
    const std::vector<long long> &dimensions, size_t depth, long long begin,
    std::vector<long long> &values) {
  if (!isInitializerList(node)) {
    if (begin < 0 || begin >= static_cast<long long>(values.size())) {
      throw IrError("too many global array initializer values");
    }
    values[begin] =
        expectConstant(emitGlobalExpression(node), "global array initializer");
    return begin + 1;
  }

  std::vector<const compiler::parser::ParseNode *> children;
  collectInitializerChildren(*node.children[1], children);
  long long cursor = begin;
  long long limit = begin + elementCount(dimensions, depth);
  for (const compiler::parser::ParseNode *child : children) {
    if (isInitializerList(*child) && depth + 1 < dimensions.size()) {
      size_t child_depth = depth + 1;
      for (; child_depth < dimensions.size(); ++child_depth) {
        long long sub_size = elementCount(dimensions, child_depth);
        if ((cursor - begin) % sub_size == 0) {
          break;
        }
      }
      cursor = fillGlobalInitializer(*child, dimensions, child_depth, cursor,
                                     values);
    } else {
      cursor = fillGlobalInitializer(*child, dimensions, dimensions.size(),
                                     cursor, values);
    }
    if (cursor > limit) {
      throw IrError("too many global array initializer values");
    }
  }
  return limit;
}

std::string ProgramBuilder::formatArrayInitializer(
    const std::vector<long long> &values,
    const std::vector<long long> &dimensions, size_t depth, long long begin) {
  if (depth == dimensions.size()) {
    return std::to_string(values[begin]);
  }
  long long sub_size = elementCount(dimensions, depth + 1);
  std::ostringstream output;
  output << "{";
  for (long long i = 0; i < dimensions[depth]; ++i) {
    if (i != 0) {
      output << ", ";
    }
    output << formatArrayInitializer(values, dimensions, depth + 1,
                                     begin + i * sub_size);
  }
  output << "}";
  return output.str();
}

} // namespace compiler::ir
