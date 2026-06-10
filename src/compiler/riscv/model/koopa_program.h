#pragma once

#include <string>
#include <vector>

namespace compiler::riscv {

struct GlobalVariable {
  std::string name;
  std::vector<int> initial_values;
  std::vector<int> dimensions;
};

struct Function {
  std::string name;
  std::vector<std::string> params;
  std::vector<std::string> param_types;
  std::vector<std::string> instructions;
};

struct Program {
  std::vector<GlobalVariable> globals;
  std::vector<Function> functions;
};

struct CallInstruction {
  bool has_result = false;
  std::string result;
  std::string callee;
  std::vector<std::string> args;
};

Program parseProgram(const std::string &koopa_ir);
CallInstruction parseCallInstruction(const std::string &line);

} // namespace compiler::riscv
