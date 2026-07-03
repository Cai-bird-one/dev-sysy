#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/emit/expression/expression_nodes.h"
#include "compiler/ir/emit/expression/runtime_expression_translator.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <sstream>

namespace compiler::ir {

Value FunctionBodyEmitter::emitExpression(const compiler::parser::ParseNode &node) {
  RuntimeExpressionTranslator translator(*this);
  return translator.translate(node);
}

Value FunctionBodyEmitter::emitCall(const compiler::parser::ParseNode &node) {
  const std::string &name = node.children[0]->lexeme;
  const FunctionSignature &signature = context_.lookupFunction(name);
  if (signature.external) {
    context_.markExternalFunctionUsed(name);
  }

  std::vector<const compiler::parser::ParseNode *> arg_nodes;
  collectCallArgumentNodes(*node.children[2], arg_nodes);
  if (arg_nodes.size() != signature.parameter_count) {
    throw IrError("argument count mismatch for function: " + name);
  }
  std::vector<Value> args;
  for (size_t i = 0; i < arg_nodes.size(); ++i) {
    std::string parameter_type =
        i < signature.parameter_types.size()
            ? signature.parameter_types[i]
            : std::string("i32");
    if (startsWith(parameter_type, "*")) {
      args.push_back(emitPointerArgument(*arg_nodes[i], parameter_type));
    } else {
      args.push_back(emitExpression(*arg_nodes[i]));
    }
  }

  std::ostringstream call;
  call << "call @" << name << "(";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      call << ", ";
    }
    call << args[i].operand;
  }
  call << ")";

  if (signature.return_type == "void") {
    emit(call.str());
    return makeConstant(0);
  }

  std::string result = newTemp();
  emit(result + " = " + call.str());
  return Value{false, 0, result};
}

void FunctionBodyEmitter::collectCallArgumentNodes(
    const compiler::parser::ParseNode &node,
    std::vector<const compiler::parser::ParseNode *> &args) {
  if (node.children.empty()) {
    return;
  }
  if (node.symbol == "FuncRParamsOpt") {
    collectCallArgumentNodes(*node.children[0], args);
    return;
  }
  if (node.symbol == "FuncRParams") {
    args.push_back(node.children[0].get());
    collectCallArgumentNodes(*node.children[1], args);
    return;
  }
  if (node.symbol == "FuncRParamsTail") {
    if (node.children.empty()) {
      return;
    }
    for (size_t i = 0; i < node.children.size();) {
      if (i + 1 >= node.children.size() ||
          node.children[i]->symbol != "COMMA") {
        throw IrError("invalid FuncRParamsTail node");
      }
      args.push_back(node.children[i + 1].get());
      i += 2;
      if (i < node.children.size() &&
          node.children[i]->symbol == "FuncRParamsTail") {
        collectCallArgumentNodes(*node.children[i], args);
        ++i;
      }
    }
    return;
  }
  throw IrError("invalid function argument node: " + node.symbol);
}

const compiler::parser::ParseNode *FunctionBodyEmitter::unwrapArrayArgument(
    const compiler::parser::ParseNode &node) {
  if (node.symbol == "LVal") {
    return &node;
  }
  if (isExpressionWrapper(node.symbol) && node.children.size() == 1) {
    return unwrapArrayArgument(*node.children[0]);
  }
  if (node.symbol == "PrimaryExp") {
    if (node.children.size() == 1) {
      return unwrapArrayArgument(*node.children[0]);
    }
    if (node.children.size() == 3 && node.children[0]->symbol == "LPAREN") {
      return unwrapArrayArgument(*node.children[1]);
    }
  }
  if (node.symbol == "UnaryExp" && node.children.size() == 1) {
    return unwrapArrayArgument(*node.children[0]);
  }
  if (isBinaryExpression(node.symbol) && node.children.size() == 2 &&
      node.children[1]->children.empty()) {
    return unwrapArrayArgument(*node.children[0]);
  }
  return nullptr;
}

} // namespace compiler::ir
