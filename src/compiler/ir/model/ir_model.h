#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::ir {

enum class SymbolKind {
  Constant,
  Variable,
};

struct Symbol {
  SymbolKind kind = SymbolKind::Constant;
  long long const_value = 0;
  std::string pointer;
  std::vector<long long> dimensions;
  bool assignable = false;
  bool pointer_parameter = false;
};

struct FunctionSignature {
  std::string return_type = "i32";
  size_t parameter_count = 0;
  bool external = false;
  std::vector<std::string> parameter_types;
};

struct SemanticProgram {
  std::map<std::string, FunctionSignature> function_signatures;
  std::set<std::string> reserved_values;
};

struct Value {
  bool constant = false;
  long long const_value = 0;
  std::string operand;
};

struct ConstValue {
  long long value = 0;
};

} // namespace compiler::ir
