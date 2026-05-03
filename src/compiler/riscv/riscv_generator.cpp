#include "compiler/riscv/riscv_generator.h"

#include <ostream>
#include <regex>
#include <sstream>

namespace compiler::riscv {
namespace {

bool isIntegerLiteral(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  size_t start = 0;
  if (text[0] == '-') {
    if (text.size() == 1) {
      return false;
    }
    start = 1;
  }
  for (size_t i = start; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
  }
  return true;
}

} // namespace

std::string RiscvGenerator::generate(const std::string &koopa_ir) const {
  std::ostringstream output;
  generate(koopa_ir, output);
  return output.str();
}

void RiscvGenerator::generate(const std::string &koopa_ir,
                              std::ostream &output) const {
  ReturnFunction function = parseReturnFunction(koopa_ir);

  output << "  .text\n"
         << "  .globl " << function.name << "\n"
         << function.name << ":\n"
         << "  li a0, " << function.value << "\n"
         << "  ret\n";
}

RiscvGenerator::ReturnFunction
RiscvGenerator::parseReturnFunction(const std::string &koopa_ir) const {
  static const std::regex function_regex(
      R"(fun\s+@([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*:\s*i32\s*\{)");
  static const std::regex return_regex(R"(\bret\s+(-?[0-9]+)\b)");

  std::smatch function_match;
  if (!std::regex_search(koopa_ir, function_match, function_regex)) {
    throw RiscvError("unsupported Koopa IR: expected i32 function definition");
  }

  size_t body_start =
      static_cast<size_t>(function_match.position() + function_match.length());
  std::string body = koopa_ir.substr(body_start);
  std::smatch return_match;
  if (!std::regex_search(body, return_match, return_regex)) {
    throw RiscvError("unsupported Koopa IR: expected integer return");
  }

  std::string value = return_match[1].str();
  if (!isIntegerLiteral(value)) {
    throw RiscvError("invalid return integer literal: " + value);
  }

  return {function_match[1].str(), value};
}

} // namespace compiler::riscv
