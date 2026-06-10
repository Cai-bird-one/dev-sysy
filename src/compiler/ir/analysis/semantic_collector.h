#pragma once

#include "compiler/ir/model/ir_model.h"
#include "compiler/parser/parser.h"

#include <map>
#include <string>
#include <vector>

namespace compiler::ir {

class FunctionSemanticCollector {
public:
  explicit FunctionSemanticCollector(
      const std::map<std::string, Symbol> &global_symbols);

  SemanticProgram analyze(const compiler::parser::ParseNode &ast);

private:
  void registerBuiltinFunctions();
  size_t countFunctionParameters(const compiler::parser::ParseNode &function);
  std::vector<std::string> collectFunctionParameterTypes(
      const compiler::parser::ParseNode &function);
  Value evaluateConstantExpression(const compiler::parser::ParseNode &node);
  const Symbol &lookupGlobal(const std::string &name) const;

  const std::map<std::string, Symbol> &global_symbols_;
  SemanticProgram program_;
};

} // namespace compiler::ir
