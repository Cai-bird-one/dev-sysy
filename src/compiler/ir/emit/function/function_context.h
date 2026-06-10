#pragma once

#include "compiler/ir/emit/statement/statement_translator.h"
#include "compiler/ir/model/ir_model.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::ir {

class FunctionContext {
public:
  FunctionContext(std::map<std::string, Symbol> global_symbols,
                  std::map<std::string, FunctionSignature>
                      function_signatures,
                  std::set<std::string> reserved_values,
                  std::set<std::string> &used_external_functions,
                  std::string function_name, std::string return_type);

  const FunctionSignature &lookupFunction(const std::string &name) const;
  void markExternalFunctionUsed(const std::string &name);
  Value makeConstant(long long value) const;
  void emit(std::string instruction);
  void emitLocalAlloc(std::string instruction);
  void emitLabel(const std::string &label);
  bool blockTerminated() const;
  const std::vector<std::string> &entryAllocs() const;
  const std::vector<std::string> &instructions() const;
  std::string newTemp();
  std::string newLabel(const std::string &prefix);
  std::string newNamedValue(const std::string &name);
  std::string newParameterValue(const std::string &name);
  void define(const std::string &name, Symbol symbol);
  const Symbol &lookup(const std::string &name) const;
  void pushScope();
  void popScope();
  const std::string &returnType() const;
  const std::string &functionName() const;
  void markReturned();
  bool hasLoop() const;
  StatementLoopLabels currentLoop() const;
  void pushLoop(StatementLoopLabels labels);
  void popLoop();

private:
  bool isTerminator(const std::string &instruction) const;

  std::map<std::string, FunctionSignature> function_signatures_;
  std::set<std::string> &used_external_functions_;
  std::string function_name_;
  std::string return_type_;
  std::vector<std::string> entry_allocs_;
  std::vector<std::string> instructions_;
  std::vector<std::map<std::string, Symbol>> scopes_;
  std::set<std::string> used_values_;
  std::vector<StatementLoopLabels> loop_stack_;
  int temp_id_ = 0;
  int label_id_ = 0;
  bool block_terminated_ = false;
  bool returned_ = false;
};

} // namespace compiler::ir
