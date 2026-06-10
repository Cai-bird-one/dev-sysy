#include "compiler/riscv/model/koopa_program.h"

#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

#include <sstream>
#include <utility>

namespace compiler::riscv {
namespace {

std::vector<std::string> parseFunctionParamTypes(const std::string &line) {
  size_t left = line.find('(');
  size_t right = line.find(')', left);
  if (left == std::string::npos || right == std::string::npos || right < left) {
    throw RiscvError("invalid function header: " + line);
  }
  std::string params_text = trim(line.substr(left + 1, right - left - 1));
  if (params_text.empty()) {
    return {};
  }

  std::vector<std::string> types;
  for (const std::string &param : splitCommaList(params_text)) {
    size_t colon = param.find(':');
    if (colon == std::string::npos) {
      throw RiscvError("invalid function parameter: " + param);
    }
    std::string type = trim(param.substr(colon + 1));
    if (type != "i32" && !startsWith(type, "*")) {
      throw RiscvError("unsupported function parameter type: " + param);
    }
    types.push_back(type);
  }
  return types;
}

std::vector<std::string> parseFunctionParams(const std::string &line) {
  size_t left = line.find('(');
  size_t right = line.find(')', left);
  if (left == std::string::npos || right == std::string::npos || right < left) {
    throw RiscvError("invalid function header: " + line);
  }
  std::string params_text = trim(line.substr(left + 1, right - left - 1));
  if (params_text.empty()) {
    return {};
  }

  std::vector<std::string> params;
  for (const std::string &param : splitCommaList(params_text)) {
    size_t colon = param.find(':');
    if (colon == std::string::npos) {
      throw RiscvError("invalid function parameter: " + param);
    }
    std::string name = trim(param.substr(0, colon));
    std::string type = trim(param.substr(colon + 1));
    if (type != "i32" && !startsWith(type, "*")) {
      throw RiscvError("unsupported function parameter type: " + param);
    }
    params.push_back(name);
  }
  return params;
}

} // namespace

CallInstruction parseCallInstruction(const std::string &line) {
  CallInstruction call;
  size_t call_pos = line.find("call @");
  if (call_pos == std::string::npos) {
    throw RiscvError("invalid call instruction: " + line);
  }

  size_t eq_pos = line.find(" = ");
  if (eq_pos != std::string::npos && eq_pos < call_pos) {
    call.has_result = true;
    call.result = trim(line.substr(0, eq_pos));
  }

  size_t name_begin = call_pos + 5;
  size_t left = line.find('(', name_begin);
  size_t right = line.rfind(')');
  if (left == std::string::npos || right == std::string::npos || right < left) {
    throw RiscvError("invalid call instruction: " + line);
  }
  call.callee = line.substr(name_begin, left - name_begin);
  call.args = splitCommaList(trim(line.substr(left + 1, right - left - 1)));
  return call;
}

Program parseProgram(const std::string &koopa_ir) {
  Program program;
  std::istringstream input(koopa_ir);
  std::string line;
  bool in_function = false;
  Function current_function;

  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (startsWith(line, "global ")) {
      size_t name_begin = std::string("global ").size();
      size_t name_end = line.find(" = alloc ", name_begin);
      if (name_end == std::string::npos) {
        throw RiscvError("unsupported global Koopa IR: " + line);
      }
      std::string name =
          stripSigil(line.substr(name_begin, name_end - name_begin));
      size_t type_begin = name_end + std::string(" = alloc ").size();
      size_t comma = findTopLevelComma(line, type_begin);
      if (comma == std::string::npos) {
        throw RiscvError("unsupported global Koopa IR: " + line);
      }
      std::string type = trim(line.substr(type_begin, comma - type_begin));
      std::string initializer = trim(line.substr(comma + 1));
      std::vector<int> dimensions = parseTypeDimensions(type);
      int count = dimensions.empty() ? 1 : elementCount(dimensions);
      std::vector<int> values = parseInitializerValues(initializer, count);
      program.globals.push_back({name, values, dimensions});
      continue;
    }

    if (startsWith(line, "fun @")) {
      size_t name_begin = line.find('@') + 1;
      size_t name_end = line.find('(', name_begin);
      if (name_begin == std::string::npos || name_end == std::string::npos ||
          name_end <= name_begin) {
        throw RiscvError("invalid function header: " + line);
      }
      current_function = Function{};
      current_function.name = line.substr(name_begin, name_end - name_begin);
      current_function.params = parseFunctionParams(line);
      current_function.param_types = parseFunctionParamTypes(line);
      in_function = true;
      continue;
    }

    if (line == "}" && in_function) {
      program.functions.push_back(std::move(current_function));
      in_function = false;
      continue;
    }

    if (!in_function) {
      continue;
    }

    current_function.instructions.push_back(line);
  }

  if (program.functions.empty()) {
    throw RiscvError("unsupported Koopa IR: expected function definition");
  }
  return program;
}

} // namespace compiler::riscv
