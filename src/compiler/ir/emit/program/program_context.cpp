#include "compiler/ir/emit/program/program_context.h"

#include "compiler/ir/koopa_generator.h"

#include <utility>

namespace compiler::ir {

void ProgramContext::addGlobalInstruction(std::string instruction) {
  global_instructions_.push_back(std::move(instruction));
}

const std::vector<std::string> &ProgramContext::globalInstructions() const {
  return global_instructions_;
}

std::string ProgramContext::newGlobalValue(const std::string &name) {
  std::string base = "@" + name;
  std::string candidate = base;
  int suffix = 0;
  while (used_values_.find(candidate) != used_values_.end()) {
    candidate = base + "_" + std::to_string(++suffix);
  }
  used_values_.insert(candidate);
  return candidate;
}

void ProgramContext::defineGlobal(const std::string &name, Symbol symbol) {
  if (global_symbols_.find(name) != global_symbols_.end()) {
    throw IrError("duplicate identifier in global scope: " + name);
  }
  global_symbols_[name] = std::move(symbol);
}

const Symbol &ProgramContext::lookupGlobal(const std::string &name) const {
  auto found = global_symbols_.find(name);
  if (found == global_symbols_.end()) {
    throw IrError("unknown global identifier: " + name);
  }
  return found->second;
}

const std::map<std::string, Symbol> &ProgramContext::globalSymbols() const {
  return global_symbols_;
}

void ProgramContext::setFunctionSignatures(
    std::map<std::string, FunctionSignature> function_signatures) {
  function_signatures_ = std::move(function_signatures);
}

const std::map<std::string, FunctionSignature> &
ProgramContext::functionSignatures() const {
  return function_signatures_;
}

const FunctionSignature &
ProgramContext::functionSignature(const std::string &name) const {
  return function_signatures_.at(name);
}

void ProgramContext::addReservedValues(
    const std::set<std::string> &reserved_values) {
  used_values_.insert(reserved_values.begin(), reserved_values.end());
}

std::set<std::string> &ProgramContext::usedValues() { return used_values_; }

std::set<std::string> &ProgramContext::usedExternalFunctions() {
  return used_external_functions_;
}

const std::set<std::string> &ProgramContext::usedExternalFunctions() const {
  return used_external_functions_;
}

} // namespace compiler::ir
