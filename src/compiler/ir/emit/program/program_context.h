#pragma once

#include "compiler/ir/model/ir_model.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::ir {

class ProgramContext {
public:
  void addGlobalInstruction(std::string instruction);
  const std::vector<std::string> &globalInstructions() const;

  std::string newGlobalValue(const std::string &name);
  void defineGlobal(const std::string &name, Symbol symbol);
  const Symbol &lookupGlobal(const std::string &name) const;
  const std::map<std::string, Symbol> &globalSymbols() const;

  void setFunctionSignatures(
      std::map<std::string, FunctionSignature> function_signatures);
  const std::map<std::string, FunctionSignature> &functionSignatures() const;
  const FunctionSignature &functionSignature(const std::string &name) const;

  void addReservedValues(const std::set<std::string> &reserved_values);
  std::set<std::string> &usedValues();
  std::set<std::string> &usedExternalFunctions();
  const std::set<std::string> &usedExternalFunctions() const;

private:
  std::vector<std::string> global_instructions_;
  std::map<std::string, Symbol> global_symbols_;
  std::map<std::string, FunctionSignature> function_signatures_;
  std::set<std::string> used_external_functions_;
  std::set<std::string> used_values_;
};

} // namespace compiler::ir
