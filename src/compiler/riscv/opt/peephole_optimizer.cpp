#include "compiler/riscv/opt/peephole_optimizer.h"

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace compiler::riscv {
namespace {

struct MemoryAccess {
  bool valid = false;
  std::string opcode;
  std::string reg;
  std::string address;
};

struct OptimizerState {
  std::unordered_map<std::string, std::string> stack_value_regs;

  void clear() { stack_value_regs.clear(); }

  void clearStackValues() { stack_value_regs.clear(); }

  void invalidateRegister(const std::string &reg) {
    for (auto iter = stack_value_regs.begin(); iter != stack_value_regs.end();) {
      if (iter->second == reg) {
        iter = stack_value_regs.erase(iter);
      } else {
        ++iter;
      }
    }
  }
};

std::string trim(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    lines.push_back(line);
  }
  return lines;
}

std::string joinLines(const std::vector<std::string> &lines,
                      bool trailing_newline) {
  std::ostringstream output;
  for (const std::string &line : lines) {
    output << line << '\n';
  }
  if (!trailing_newline && !lines.empty()) {
    std::string result = output.str();
    result.pop_back();
    return result;
  }
  return output.str();
}

MemoryAccess parseMemoryAccess(const std::string &line) {
  std::string text = trim(line);
  if (!startsWith(text, "lw ") && !startsWith(text, "sw ")) {
    return {};
  }

  size_t comma = text.find(',');
  if (comma == std::string::npos) {
    return {};
  }

  MemoryAccess access;
  access.valid = true;
  access.opcode = text.substr(0, 2);
  access.reg = trim(text.substr(3, comma - 3));
  access.address = trim(text.substr(comma + 1));
  return access;
}

std::vector<std::string> parseOperands(const std::string &line) {
  std::string text = trim(line);
  size_t space = text.find(' ');
  if (space == std::string::npos) {
    return {};
  }

  std::vector<std::string> operands;
  std::string rest = text.substr(space + 1);
  size_t begin = 0;
  while (begin <= rest.size()) {
    size_t comma = rest.find(',', begin);
    if (comma == std::string::npos) {
      operands.push_back(trim(rest.substr(begin)));
      break;
    }
    operands.push_back(trim(rest.substr(begin, comma - begin)));
    begin = comma + 1;
  }
  return operands;
}

std::string opcodeOf(const std::string &line) {
  std::string text = trim(line);
  size_t space = text.find(' ');
  if (space == std::string::npos) {
    return text;
  }
  return text.substr(0, space);
}

bool definesFirstOperand(const std::string &opcode) {
  return opcode == "li" || opcode == "la" || opcode == "lw" ||
         opcode == "mv" || opcode == "add" || opcode == "addi" ||
         opcode == "sub" || opcode == "mul" || opcode == "div" ||
         opcode == "rem" || opcode == "slt" || opcode == "xori" ||
         opcode == "seqz" || opcode == "snez" || opcode == "xor" ||
         opcode == "or" || opcode == "and" || opcode == "slli" ||
         opcode == "srli" || opcode == "srai";
}

std::string definedRegister(const std::string &line) {
  std::string opcode = opcodeOf(line);
  if (!definesFirstOperand(opcode)) {
    return "";
  }
  std::vector<std::string> operands = parseOperands(line);
  if (operands.empty()) {
    return "";
  }
  return operands[0];
}

bool isBoundary(const std::string &line) {
  std::string text = trim(line);
  if (text.empty()) {
    return false;
  }
  if (text.back() == ':' || text[0] == '.') {
    return true;
  }
  std::string opcode = opcodeOf(text);
  return opcode == "call" || opcode == "ret" || opcode == "j" ||
         opcode == "jal" || opcode == "jalr" || startsWith(opcode, "b");
}

bool isStackAddress(const std::string &address) {
  return address.size() > 4 && address.substr(address.size() - 4) == "(sp)";
}

std::string indentLikeInstruction(const std::string &instruction) {
  return "  " + instruction;
}

bool isNoOp(const std::string &line) {
  std::string text = trim(line);
  if (startsWith(text, "mv ")) {
    size_t comma = text.find(',');
    if (comma == std::string::npos) {
      return false;
    }
    std::string dst = trim(text.substr(3, comma - 3));
    std::string src = trim(text.substr(comma + 1));
    return dst == src;
  }
  if (startsWith(text, "addi ")) {
    size_t first_comma = text.find(',');
    size_t second_comma = text.find(',', first_comma + 1);
    if (first_comma == std::string::npos || second_comma == std::string::npos) {
      return false;
    }
    std::string dst = trim(text.substr(5, first_comma - 5));
    std::string src =
        trim(text.substr(first_comma + 1, second_comma - first_comma - 1));
    std::string imm = trim(text.substr(second_comma + 1));
    return dst == src && imm == "0";
  }
  return false;
}

std::string simplifyArithmetic(const std::string &line) {
  std::string opcode = opcodeOf(line);
  std::vector<std::string> operands = parseOperands(line);
  if (operands.size() != 3) {
    return line;
  }

  const std::string &dst = operands[0];
  const std::string &lhs = operands[1];
  const std::string &rhs = operands[2];
  if ((opcode == "add" || opcode == "or" || opcode == "xor") &&
      rhs == "zero") {
    return indentLikeInstruction("mv " + dst + ", " + lhs);
  }
  if ((opcode == "add" || opcode == "or" || opcode == "xor") &&
      lhs == "zero") {
    return indentLikeInstruction("mv " + dst + ", " + rhs);
  }
  if (opcode == "sub" && rhs == "zero") {
    return indentLikeInstruction("mv " + dst + ", " + lhs);
  }
  if (opcode == "mul" && (lhs == "zero" || rhs == "zero")) {
    return indentLikeInstruction("li " + dst + ", 0");
  }
  if (opcode == "addi" && rhs == "0") {
    return indentLikeInstruction("mv " + dst + ", " + lhs);
  }
  return line;
}

bool parsePowerOfTwoLoad(const std::string &line, int &shift) {
  std::string text = trim(line);
  if (!startsWith(text, "li t2, ")) {
    return false;
  }
  int value = 0;
  try {
    value = std::stoi(trim(text.substr(7)));
  } catch (...) {
    return false;
  }
  if (value <= 0 || (value & (value - 1)) != 0) {
    return false;
  }
  shift = 0;
  while (value > 1) {
    value >>= 1;
    ++shift;
  }
  return true;
}

bool isT2ScaleMultiply(const std::string &line) {
  return trim(line) == "mul t1, t1, t2";
}

bool isLabel(const std::string &line, std::string &label) {
  std::string text = trim(line);
  if (text.empty() || text.back() != ':') {
    return false;
  }
  label = text.substr(0, text.size() - 1);
  return true;
}

bool isJumpToLabel(const std::string &line, const std::string &label) {
  return trim(line) == "j " + label;
}

} // namespace

std::string PeepholeOptimizer::optimize(const std::string &assembly) const {
  std::vector<std::string> input = splitLines(assembly);
  std::vector<std::string> output;
  OptimizerState state;

  for (std::string line : input) {
    if (!output.empty()) {
      std::string label;
      if (isLabel(line, label) && isJumpToLabel(output.back(), label)) {
        output.pop_back();
      }
    }

    if (isBoundary(line)) {
      state.clear();
      output.push_back(std::move(line));
      continue;
    }

    line = simplifyArithmetic(line);
    if (isNoOp(line)) {
      continue;
    }

    if (!output.empty()) {
      int shift = 0;
      if (parsePowerOfTwoLoad(output.back(), shift) && isT2ScaleMultiply(line)) {
        state.invalidateRegister("t1");
        output.push_back(indentLikeInstruction("slli t1, t1, " +
                                               std::to_string(shift)));
        continue;
      }
    }

    MemoryAccess access = parseMemoryAccess(line);
    if (access.valid) {
      if (!isStackAddress(access.address)) {
        if (access.opcode == "sw") {
          state.clearStackValues();
        }
      } else if (access.opcode == "lw") {
        auto iter = state.stack_value_regs.find(access.address);
        if (iter != state.stack_value_regs.end()) {
          if (iter->second == access.reg) {
            continue;
          }
          line = indentLikeInstruction("mv " + access.reg + ", " +
                                       iter->second);
        }
        state.invalidateRegister(access.reg);
        state.stack_value_regs[access.address] = access.reg;
        output.push_back(std::move(line));
        continue;
      } else {
        state.stack_value_regs[access.address] = access.reg;
        output.push_back(std::move(line));
        continue;
      }
    }

    std::string def = definedRegister(line);
    if (!def.empty()) {
      state.invalidateRegister(def);
    }

    output.push_back(std::move(line));
  }

  bool trailing_newline = !assembly.empty() && assembly.back() == '\n';
  return joinLines(output, trailing_newline);
}

} // namespace compiler::riscv
