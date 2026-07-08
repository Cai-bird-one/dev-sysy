#include "compiler/riscv/opt/peephole_optimizer.h"

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace compiler::riscv {
namespace {

struct MemoryAccess {
  bool valid = false;
  std::string opcode;
  std::string reg;
  std::string address;
};

struct MoveInstruction {
  bool valid = false;
  std::string dst;
  std::string src;
};

struct ConditionalBranch {
  bool valid = false;
  std::string opcode;
  std::string reg;
  std::string label;
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

MoveInstruction parseMove(const std::string &line) {
  if (opcodeOf(line) != "mv") {
    return {};
  }
  std::vector<std::string> operands = parseOperands(line);
  if (operands.size() != 2) {
    return {};
  }
  return {true, operands[0], operands[1]};
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

std::string nopInstruction() { return indentLikeInstruction("nop"); }

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

bool parseJumpTarget(const std::string &line, std::string &label) {
  std::string text = trim(line);
  if (!startsWith(text, "j ")) {
    return false;
  }
  label = trim(text.substr(2));
  return !label.empty();
}

ConditionalBranch parseConditionalBranch(const std::string &line) {
  std::string opcode = opcodeOf(line);
  if (opcode != "bnez" && opcode != "beqz") {
    return {};
  }
  std::vector<std::string> operands = parseOperands(line);
  if (operands.size() != 2) {
    return {};
  }
  return {true, opcode, operands[0], operands[1]};
}

std::string invertedBranchOpcode(const std::string &opcode) {
  return opcode == "bnez" ? "beqz" : "bnez";
}

bool isInstructionLine(const std::string &line) {
  std::string text = trim(line);
  return !text.empty() && text.back() != ':' && text[0] != '.';
}

bool isLayoutSensitiveFunction(int instruction_count, int branch_count) {
  if (instruction_count >= 1200 && branch_count >= 20) {
    return true;
  }
  return branch_count >= 3 && branch_count * 13 >= instruction_count;
}

void markFunctionSensitivity(std::vector<bool> &sensitive, size_t begin,
                             size_t end, int instruction_count,
                             int branch_count) {
  if (begin >= end ||
      !isLayoutSensitiveFunction(instruction_count, branch_count)) {
    return;
  }
  for (size_t i = begin; i < end; ++i) {
    sensitive[i] = true;
  }
}

std::vector<bool>
computeLayoutSensitiveLines(const std::vector<std::string> &lines) {
  std::vector<bool> sensitive(lines.size(), false);
  bool in_text = false;
  std::string pending_global;
  size_t function_begin = lines.size();
  int instruction_count = 0;
  int branch_count = 0;

  for (size_t i = 0; i < lines.size(); ++i) {
    std::string text = trim(lines[i]);
    if (text == ".text") {
      in_text = true;
      continue;
    }
    if (!in_text) {
      continue;
    }

    if (startsWith(text, ".globl ")) {
      markFunctionSensitivity(sensitive, function_begin, i, instruction_count,
                              branch_count);
      function_begin = lines.size();
      instruction_count = 0;
      branch_count = 0;
      pending_global = trim(text.substr(std::string(".globl ").size()));
      continue;
    }

    if (!pending_global.empty() && text == pending_global + ":") {
      function_begin = i;
      pending_global.clear();
    }

    if (function_begin != lines.size() && isInstructionLine(lines[i])) {
      ++instruction_count;
      std::string opcode = opcodeOf(lines[i]);
      if (opcode == "j" || startsWith(opcode, "b")) {
        ++branch_count;
      }
    }
  }

  markFunctionSensitivity(sensitive, function_begin, lines.size(),
                          instruction_count, branch_count);
  return sensitive;
}

bool isRedundantAfterMove(const MoveInstruction &previous,
                          const MoveInstruction &current) {
  if (!previous.valid || !current.valid) {
    return false;
  }
  if (previous.dst == current.dst && previous.src == current.src) {
    return true;
  }
  return previous.dst == current.src && previous.src == current.dst;
}

std::string runPeepholePass(const std::string &assembly) {
  std::vector<std::string> input = splitLines(assembly);
  std::vector<bool> layout_sensitive = computeLayoutSensitiveLines(input);
  std::vector<std::string> output;
  std::vector<bool> output_sensitive;
  OptimizerState state;

  for (size_t index = 0; index < input.size(); ++index) {
    std::string line = input[index];
    bool allow_layout_sensitive_rewrite = !layout_sensitive[index];

    if (!output.empty()) {
      std::string label;
      if (isLabel(line, label) && isJumpToLabel(output.back(), label)) {
        if (output_sensitive.back()) {
          output.back() = nopInstruction();
        } else {
          output.pop_back();
          output_sensitive.pop_back();
        }
      }
    }
    if (allow_layout_sensitive_rewrite && output.size() >= 2 &&
        !output_sensitive[output.size() - 1] &&
        !output_sensitive[output.size() - 2]) {
      std::string label;
      std::string jump_target;
      ConditionalBranch branch =
          parseConditionalBranch(output[output.size() - 2]);
      if (isLabel(line, label) && branch.valid && branch.label == label &&
          parseJumpTarget(output.back(), jump_target)) {
        output.pop_back();
        output_sensitive.pop_back();
        std::string opcode = invertedBranchOpcode(branch.opcode);
        output.back() = indentLikeInstruction(opcode + " " + branch.reg +
                                              ", " + jump_target);
      }
    }

    if (isBoundary(line)) {
      state.clear();
      output.push_back(std::move(line));
      output_sensitive.push_back(layout_sensitive[index]);
      continue;
    }

    line = simplifyArithmetic(line);
    if (isNoOp(line)) {
      if (!layout_sensitive[index]) {
        continue;
      }
      line = nopInstruction();
    }

    MoveInstruction move = parseMove(line);
    if (!output.empty() && move.valid &&
        isRedundantAfterMove(parseMove(output.back()), move)) {
      if (allow_layout_sensitive_rewrite) {
        continue;
      }
      line = nopInstruction();
      move = {};
    }

    if (!output.empty()) {
      int shift = 0;
      if (parsePowerOfTwoLoad(output.back(), shift) && isT2ScaleMultiply(line)) {
        state.invalidateRegister("t1");
        output.push_back(indentLikeInstruction("slli t1, t1, " +
                                               std::to_string(shift)));
        output_sensitive.push_back(layout_sensitive[index]);
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
        output_sensitive.push_back(layout_sensitive[index]);
        continue;
      } else {
        state.stack_value_regs[access.address] = access.reg;
        output.push_back(std::move(line));
        output_sensitive.push_back(layout_sensitive[index]);
        continue;
      }
    }

    std::string def = definedRegister(line);
    if (!def.empty()) {
      state.invalidateRegister(def);
    }

    output.push_back(std::move(line));
    output_sensitive.push_back(layout_sensitive[index]);
  }

  bool trailing_newline = !assembly.empty() && assembly.back() == '\n';
  return joinLines(output, trailing_newline);
}

} // namespace

const char *PeepholeOptimizer::name() const { return "peephole"; }

std::string PeepholeOptimizer::run(const std::string &assembly) const {
  return runPeepholePass(assembly);
}

std::string PeepholeOptimizer::optimize(const std::string &assembly) const {
  return run(assembly);
}

} // namespace compiler::riscv
