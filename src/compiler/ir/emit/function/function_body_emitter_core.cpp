#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <utility>

namespace compiler::ir {

FunctionBodyEmitter::FunctionBodyEmitter(
    std::map<std::string, Symbol> global_symbols,
    std::map<std::string, FunctionSignature> function_signatures,
    std::set<std::string> reserved_values,
    std::set<std::string> &used_external_functions, std::string function_name,
    std::string return_type)
    : context_(std::move(global_symbols), std::move(function_signatures),
               std::move(reserved_values), used_external_functions,
               std::move(function_name), std::move(return_type)) {}

Value FunctionBodyEmitter::makeConstant(long long value) const {
  return context_.makeConstant(value);
}

void FunctionBodyEmitter::emit(std::string instruction) {
  context_.emit(std::move(instruction));
}

void FunctionBodyEmitter::emitLocalAlloc(std::string instruction) {
  context_.emitLocalAlloc(std::move(instruction));
}

void FunctionBodyEmitter::emitLabel(const std::string &label) {
  context_.emitLabel(label);
}

std::string FunctionBodyEmitter::newTemp() {
  return context_.newTemp();
}

std::string FunctionBodyEmitter::newLabel(const std::string &prefix) {
  return context_.newLabel(prefix);
}

std::string FunctionBodyEmitter::newNamedValue(const std::string &name) {
  return context_.newNamedValue(name);
}

std::string FunctionBodyEmitter::newParameterValue(const std::string &name) {
  return context_.newParameterValue(name);
}

void FunctionBodyEmitter::define(const std::string &name, Symbol symbol) {
  context_.define(name, std::move(symbol));
}

const Symbol &FunctionBodyEmitter::lookup(const std::string &name) const {
  return context_.lookup(name);
}

bool FunctionBodyEmitter::blockTerminated() const {
  return context_.blockTerminated();
}

const std::vector<std::string> &FunctionBodyEmitter::entryAllocs() const {
  return context_.entryAllocs();
}

const std::vector<std::string> &FunctionBodyEmitter::instructions() const {
  return context_.instructions();
}

} // namespace compiler::ir
