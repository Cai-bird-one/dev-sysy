#include "compiler/riscv/opt/peephole_optimizer.h"

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace compiler::riscv {
namespace {

struct MemoryAccess {
  bool valid = false;
  std::string opcode;
  std::string reg;
  std::string address;
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

std::string indentLikeInstruction(const std::string &instruction) {
  return "  " + instruction;
}

} // namespace

std::string PeepholeOptimizer::optimize(const std::string &assembly) const {
  std::vector<std::string> input = splitLines(assembly);
  std::vector<std::string> output;

  for (std::string line : input) {
    if (isNoOp(line)) {
      continue;
    }

    if (!output.empty()) {
      std::string label;
      if (isLabel(line, label) && isJumpToLabel(output.back(), label)) {
        output.pop_back();
      }
    }

    if (!output.empty()) {
      int shift = 0;
      if (parsePowerOfTwoLoad(output.back(), shift) && isT2ScaleMultiply(line)) {
        output.push_back(indentLikeInstruction("slli t1, t1, " +
                                               std::to_string(shift)));
        continue;
      }
    }

    if (!output.empty()) {
      MemoryAccess previous = parseMemoryAccess(output.back());
      MemoryAccess current = parseMemoryAccess(line);
      if (previous.valid && current.valid && previous.opcode == "sw" &&
          current.opcode == "lw" && previous.address == current.address) {
        if (previous.reg == current.reg) {
          continue;
        }
        line = indentLikeInstruction("mv " + current.reg + ", " + previous.reg);
      }
    }

    output.push_back(std::move(line));
  }

  bool trailing_newline = !assembly.empty() && assembly.back() == '\n';
  return joinLines(output, trailing_newline);
}

} // namespace compiler::riscv
