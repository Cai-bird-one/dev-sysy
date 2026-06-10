#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

namespace compiler::ir {

void FunctionBodyEmitter::collectInitializerChildren(
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
  throw IrError("invalid array initializer list: " + node.symbol);
}

bool FunctionBodyEmitter::isInitializerList(
    const compiler::parser::ParseNode &node) const {
  return !node.children.empty() && node.children[0]->symbol == "LBRACE";
}

long long FunctionBodyEmitter::flattenRuntimeInitializer(
    const compiler::parser::ParseNode &node,
    const std::vector<long long> &dimensions, size_t depth, long long begin,
    std::vector<std::pair<long long, Value>> &entries) {
  if (!isInitializerList(node)) {
    entries.push_back({begin, emitExpression(node)});
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
      cursor =
          flattenRuntimeInitializer(*child, dimensions, child_depth, cursor,
                                    entries);
    } else {
      cursor = flattenRuntimeInitializer(*child, dimensions, dimensions.size(),
                                         cursor, entries);
    }
    if (cursor > limit) {
      throw IrError("too many array initializer values");
    }
  }
  return limit;
}

} // namespace compiler::ir
