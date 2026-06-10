#include "compiler/ir/analysis/semantic_collector.h"

#include "compiler/ir/ast/parse_node_utils.h"
#include "compiler/ir/emit/expression/constant_expression.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <utility>

namespace compiler::ir {

FunctionSemanticCollector::FunctionSemanticCollector(
    const std::map<std::string, Symbol> &global_symbols)
    : global_symbols_(global_symbols) {}

SemanticProgram
FunctionSemanticCollector::analyze(const compiler::parser::ParseNode &ast) {
  registerBuiltinFunctions();

  std::vector<const compiler::parser::ParseNode *> functions;
  collectNodes(ast, "FuncDef", functions);
  for (const compiler::parser::ParseNode *function : functions) {
    std::string name = findFunctionName(*function);
    if (program_.function_signatures.find(name) !=
        program_.function_signatures.end()) {
      throw IrError("duplicate function: " + name);
    }
    if (global_symbols_.find(name) != global_symbols_.end()) {
      throw IrError("duplicate global identifier and function: " + name);
    }
    program_.function_signatures[name] =
        FunctionSignature{findFunctionReturnType(*function),
                          countFunctionParameters(*function), false,
                          collectFunctionParameterTypes(*function)};
    program_.reserved_values.insert("@" + name);
  }

  return std::move(program_);
}

void FunctionSemanticCollector::registerBuiltinFunctions() {
  program_.function_signatures["getint"] = FunctionSignature{"i32", 0, true, {}};
  program_.function_signatures["getch"] = FunctionSignature{"i32", 0, true, {}};
  program_.function_signatures["getarray"] =
      FunctionSignature{"i32", 1, true, {"*i32"}};
  program_.function_signatures["putint"] =
      FunctionSignature{"void", 1, true, {"i32"}};
  program_.function_signatures["putch"] =
      FunctionSignature{"void", 1, true, {"i32"}};
  program_.function_signatures["putarray"] =
      FunctionSignature{"void", 2, true, {"i32", "*i32"}};
  program_.function_signatures["starttime"] =
      FunctionSignature{"void", 0, true, {}};
  program_.function_signatures["stoptime"] =
      FunctionSignature{"void", 0, true, {}};

  program_.reserved_values.insert("@getint");
  program_.reserved_values.insert("@getch");
  program_.reserved_values.insert("@getarray");
  program_.reserved_values.insert("@putint");
  program_.reserved_values.insert("@putch");
  program_.reserved_values.insert("@putarray");
  program_.reserved_values.insert("@starttime");
  program_.reserved_values.insert("@stoptime");
}

size_t FunctionSemanticCollector::countFunctionParameters(
    const compiler::parser::ParseNode &function) {
  const compiler::parser::ParseNode *params_opt =
      findDirectChild(function, "FuncFParamsOpt");
  if (params_opt == nullptr || params_opt->children.empty() ||
      params_opt->children[0]->symbol == "VOID") {
    return 0;
  }
  std::vector<const compiler::parser::ParseNode *> params;
  collectNodes(*params_opt, "FuncFParam", params);
  return params.size();
}

std::vector<std::string> FunctionSemanticCollector::collectFunctionParameterTypes(
    const compiler::parser::ParseNode &function) {
  const compiler::parser::ParseNode *params_opt =
      findDirectChild(function, "FuncFParamsOpt");
  if (params_opt == nullptr || params_opt->children.empty() ||
      params_opt->children[0]->symbol == "VOID") {
    return {};
  }
  std::vector<const compiler::parser::ParseNode *> params;
  collectNodes(*params_opt, "FuncFParam", params);
  std::vector<std::string> types;
  for (const compiler::parser::ParseNode *param : params) {
    const compiler::parser::ParseNode *array_opt =
        findDirectChild(*param, "FuncFParamArrayOpt");
    if (array_opt == nullptr || array_opt->children.empty()) {
      types.push_back("i32");
      continue;
    }
    std::vector<long long> dimensions;
    const compiler::parser::ParseNode *current =
        findDirectChild(*array_opt, "FuncFParamArrayDims");
    while (current != nullptr && !current->children.empty()) {
      long long dimension =
          expectConstant(evaluateConstantExpression(*current->children[1]),
                         "function parameter array dimension");
      dimensions.push_back(dimension);
      current = current->children[3].get();
    }
    types.push_back("*" + arrayType(dimensions));
  }
  return types;
}

Value FunctionSemanticCollector::evaluateConstantExpression(
    const compiler::parser::ParseNode &node) {
  ConstantExpressionEvaluator evaluator(
      [this](const std::string &name) -> const Symbol & {
        return lookupGlobal(name);
      });
  return evaluator.evaluate(node);
}

const Symbol &
FunctionSemanticCollector::lookupGlobal(const std::string &name) const {
  auto found = global_symbols_.find(name);
  if (found == global_symbols_.end()) {
    throw IrError("unknown global identifier: " + name);
  }
  return found->second;
}

} // namespace compiler::ir
