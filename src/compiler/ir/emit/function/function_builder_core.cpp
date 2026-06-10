#include "compiler/ir/emit/function/function_builder.h"

#include "compiler/ir/ast/parse_node_utils.h"
#include "compiler/ir/koopa_generator.h"

#include <sstream>
#include <utility>

namespace compiler::ir {

FunctionBuilder::FunctionBuilder(
    const compiler::parser::ParseNode &function,
    std::map<std::string, Symbol> global_symbols,
    std::map<std::string, FunctionSignature> function_signatures,
    std::set<std::string> reserved_values,
    std::set<std::string> &used_external_functions)
    : function_(function), function_name_(findFunctionName(function_)),
      return_type_(findFunctionReturnType(function_)),
      body_emitter_(std::move(global_symbols), std::move(function_signatures),
                    std::move(reserved_values), used_external_functions,
                    function_name_, return_type_) {}

std::string FunctionBuilder::generate() {
  collectParameters();
  const compiler::parser::ParseNode *block = findDirectChild(function_, "Block");
  if (block == nullptr) {
    block = findFirst(function_, "Block");
  }
  if (block == nullptr) {
    throw IrError("cannot find function block in AST");
  }

  bool block_returned = body_emitter_.emitBlock(*block, false);
  if (!block_returned && !body_emitter_.blockTerminated()) {
    body_emitter_.emitInstruction(return_type_ == "void" ? std::string("ret")
                                                         : std::string("ret 0"));
  }

  std::ostringstream output;
  output << "fun @" << function_name_ << "(";
  for (size_t i = 0; i < parameters_.size(); ++i) {
    if (i != 0) {
      output << ", ";
    }
    output << parameters_[i].koopa_name << ": " << parameters_[i].type;
  }
  output << ")";
  if (return_type_ != "void") {
    output << ": " << return_type_;
  }
  output << " {\n%entry:\n";
  for (const std::string &line : body_emitter_.entryAllocs()) {
    output << "  " << line << '\n';
  }
  for (const Parameter &parameter : parameters_) {
    if (!parameter.pointer.empty()) {
      output << "  store " << parameter.koopa_name << ", " << parameter.pointer
             << '\n';
    }
  }
  for (const std::string &line : body_emitter_.instructions()) {
    output << ((!line.empty() && line.back() == ':') ? "" : "  ") << line
           << '\n';
  }
  output << "}\n";
  return output.str();
}

} // namespace compiler::ir
