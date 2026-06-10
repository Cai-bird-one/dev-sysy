#include "compiler/ir/emit/function/function_context.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <utility>

namespace compiler::ir {

FunctionContext::FunctionContext(
    std::map<std::string, Symbol> global_symbols,
    std::map<std::string, FunctionSignature> function_signatures,
    std::set<std::string> reserved_values,
    std::set<std::string> &used_external_functions, std::string function_name,
    std::string return_type)
    : function_signatures_(std::move(function_signatures)),
      used_external_functions_(used_external_functions),
      function_name_(std::move(function_name)),
      return_type_(std::move(return_type)),
      used_values_(std::move(reserved_values)) {
  scopes_.push_back(std::move(global_symbols));
  scopes_.push_back({});
}

const FunctionSignature &
FunctionContext::lookupFunction(const std::string &name) const {
  auto signature = function_signatures_.find(name);
  if (signature == function_signatures_.end()) {
    throw IrError("unknown function: " + name);
  }
  return signature->second;
}

void FunctionContext::markExternalFunctionUsed(const std::string &name) {
  used_external_functions_.insert(name);
}

Value FunctionContext::makeConstant(long long value) const {
  return Value{true, value, toOperand(value)};
}

void FunctionContext::emit(std::string instruction) {
  if (isTerminator(instruction)) {
    block_terminated_ = true;
  }
  instructions_.push_back(std::move(instruction));
}

void FunctionContext::emitLocalAlloc(std::string instruction) {
  entry_allocs_.push_back(std::move(instruction));
}

void FunctionContext::emitLabel(const std::string &label) {
  block_terminated_ = false;
  emit(label + ":");
}

bool FunctionContext::isTerminator(const std::string &instruction) const {
  return startsWith(instruction, "br ") || startsWith(instruction, "jump ") ||
         startsWith(instruction, "ret ");
}

bool FunctionContext::blockTerminated() const { return block_terminated_; }

const std::vector<std::string> &FunctionContext::entryAllocs() const {
  return entry_allocs_;
}

const std::vector<std::string> &FunctionContext::instructions() const {
  return instructions_;
}

std::string FunctionContext::newTemp() {
  return "%" + std::to_string(temp_id_++);
}

std::string FunctionContext::newLabel(const std::string &prefix) {
  return "%" + prefix + "_" + std::to_string(label_id_++);
}

std::string FunctionContext::newNamedValue(const std::string &name) {
  std::string base = "%" + name;
  std::string candidate = base;
  int suffix = 0;
  while (used_values_.find(candidate) != used_values_.end()) {
    candidate = base + "_" + std::to_string(++suffix);
  }
  used_values_.insert(candidate);
  return candidate;
}

std::string FunctionContext::newParameterValue(const std::string &name) {
  std::string base = "@" + function_name_ + "_" + name;
  std::string candidate = base;
  int suffix = 0;
  while (used_values_.find(candidate) != used_values_.end()) {
    candidate = base + "_" + std::to_string(++suffix);
  }
  used_values_.insert(candidate);
  return candidate;
}

void FunctionContext::define(const std::string &name, Symbol symbol) {
  if (scopes_.back().find(name) != scopes_.back().end()) {
    throw IrError("duplicate identifier in current scope: " + name);
  }
  scopes_.back()[name] = std::move(symbol);
}

const Symbol &FunctionContext::lookup(const std::string &name) const {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    auto found = scope->find(name);
    if (found != scope->end()) {
      return found->second;
    }
  }
  throw IrError("unknown identifier: " + name);
}

void FunctionContext::pushScope() { scopes_.push_back({}); }

void FunctionContext::popScope() { scopes_.pop_back(); }

const std::string &FunctionContext::returnType() const { return return_type_; }

const std::string &FunctionContext::functionName() const {
  return function_name_;
}

void FunctionContext::markReturned() { returned_ = true; }

bool FunctionContext::hasLoop() const { return !loop_stack_.empty(); }

StatementLoopLabels FunctionContext::currentLoop() const {
  return loop_stack_.back();
}

void FunctionContext::pushLoop(StatementLoopLabels labels) {
  loop_stack_.push_back(std::move(labels));
}

void FunctionContext::popLoop() { loop_stack_.pop_back(); }

} // namespace compiler::ir
