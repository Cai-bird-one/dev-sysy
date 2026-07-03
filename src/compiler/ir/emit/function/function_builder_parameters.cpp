#include "compiler/ir/emit/function/function_builder.h"

#include "compiler/ir/ast/parse_node_utils.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

namespace compiler::ir {

void FunctionBuilder::collectParameters() {
  const compiler::parser::ParseNode *params_opt =
      findDirectChild(function_, "FuncFParamsOpt");
  if (params_opt == nullptr || params_opt->children.empty() ||
      params_opt->children[0]->symbol == "VOID") {
    return;
  }

  std::vector<const compiler::parser::ParseNode *> param_nodes;
  collectNodes(*params_opt, "FuncFParam", param_nodes);
  for (const compiler::parser::ParseNode *param : param_nodes) {
    const compiler::parser::ParseNode *ident = findDirectChild(*param, "IDENT");
    if (ident == nullptr || ident->lexeme.empty()) {
      throw IrError("invalid function parameter");
    }
    const compiler::parser::ParseNode *btype = findDirectChild(*param, "BType");
    if (btype == nullptr) {
      throw IrError("function parameter is missing a basic type");
    }
    SourceValueType source_type = parseBType(*btype);
    std::string koopa_name = body_emitter_.newParameterValue(ident->lexeme);
    std::vector<long long> dimensions =
        collectFunctionParameterDimensions(*param);
    if (!dimensions.empty() || hasNonEmptyChild(*param, "FuncFParamArrayOpt")) {
      std::string parameter_type = "*" + arrayType(dimensions);
      body_emitter_.define(ident->lexeme,
                           Symbol{SymbolKind::Variable, 0, koopa_name,
                                  dimensions, true, true, source_type});
      parameters_.push_back(
          Parameter{ident->lexeme, koopa_name, "", parameter_type});
      continue;
    }
    if (source_type == SourceValueType::Tensor) {
      throw IrError("tensor function parameter requires array dimensions");
    }
    std::string pointer = body_emitter_.newNamedValue(ident->lexeme);
    body_emitter_.emitLocalAlloc(pointer + " = alloc i32");
    body_emitter_.define(
        ident->lexeme, Symbol{SymbolKind::Variable, 0, pointer, {}, true,
                              false, source_type});
    parameters_.push_back(Parameter{ident->lexeme, koopa_name, pointer, "i32"});
  }
}

std::vector<long long> FunctionBuilder::collectFunctionParameterDimensions(
    const compiler::parser::ParseNode &param) {
  std::vector<long long> dimensions;
  const compiler::parser::ParseNode *array_opt =
      findDirectChild(param, "FuncFParamArrayOpt");
  if (array_opt == nullptr || array_opt->children.empty()) {
    return dimensions;
  }
  const compiler::parser::ParseNode *current =
      findDirectChild(*array_opt, "FuncFParamArrayDims");
  while (current != nullptr && !current->children.empty()) {
    if (current->children.size() != 4) {
      throw IrError("invalid function parameter array dimensions");
    }
    long long dimension =
        expectConstant(body_emitter_.emitExpression(*current->children[1]),
                       "function parameter array dimension");
    if (dimension <= 0) {
      throw IrError("array dimension must be positive");
    }
    dimensions.push_back(dimension);
    current = current->children[3].get();
  }
  return dimensions;
}

} // namespace compiler::ir
