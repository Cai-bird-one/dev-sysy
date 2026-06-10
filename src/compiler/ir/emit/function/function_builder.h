#pragma once

#include "compiler/ir/emit/function/function_body_emitter.h"
#include "compiler/ir/model/ir_model.h"
#include "compiler/parser/parser.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::ir {

class FunctionBuilder {
public:
  FunctionBuilder(const compiler::parser::ParseNode &function,
                  std::map<std::string, Symbol> global_symbols,
                  std::map<std::string, FunctionSignature> function_signatures,
                  std::set<std::string> reserved_values,
                  std::set<std::string> &used_external_functions);

  std::string generate();

private:
  struct Parameter {
    std::string source_name;
    std::string koopa_name;
    std::string pointer;
    std::string type;
  };
  void collectParameters();
  std::vector<long long>
  collectFunctionParameterDimensions(const compiler::parser::ParseNode &param);

  const compiler::parser::ParseNode &function_;
  std::string function_name_;
  std::string return_type_;
  FunctionBodyEmitter body_emitter_;
  std::vector<Parameter> parameters_;
};

} // namespace compiler::ir
