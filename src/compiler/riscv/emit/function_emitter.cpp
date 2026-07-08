#include "compiler/riscv/emit/function_emitter.h"

#include "compiler/riscv/util/riscv_utils.h"

#include <ostream>
#include <string>
#include <utility>

namespace compiler::riscv {
namespace {

struct ComparisonAssignment {
  bool valid = false;
  std::string result;
};

struct BranchInstruction {
  bool valid = false;
  std::string condition;
  std::string then_label;
  std::string else_label;
};

ComparisonAssignment parseComparisonAssignment(const std::string &line) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() != 5 || parts[1] != "=" || !isComparison(parts[2])) {
    return {};
  }
  return {true, parts[0]};
}

BranchInstruction parseBranchInstruction(const std::string &line) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() != 4 || parts[0] != "br") {
    return {};
  }
  return {true, parts[1], parts[2], parts[3]};
}

bool isLiteralValue(const std::string &text, const std::string &literal) {
  return isIntegerLiteral(text) && text == literal;
}

bool parseBooleanComparison(const std::string &line,
                            const std::string &condition_value,
                            std::string &result, bool &invert) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() != 5 || parts[1] != "=") {
    return false;
  }
  const std::string &op = parts[2];
  const std::string &left = parts[3];
  const std::string &right = parts[4];
  bool value_left = left == condition_value;
  bool value_right = right == condition_value;
  if (!value_left && !value_right) {
    return false;
  }
  const std::string &literal = value_left ? right : left;
  if (op == "ne" && isLiteralValue(literal, "0")) {
    result = parts[0];
    invert = false;
    return true;
  }
  if (op == "eq" && isLiteralValue(literal, "0")) {
    result = parts[0];
    invert = true;
    return true;
  }
  if (op == "eq" && isLiteralValue(literal, "1")) {
    result = parts[0];
    invert = false;
    return true;
  }
  if (op == "ne" && isLiteralValue(literal, "1")) {
    result = parts[0];
    invert = true;
    return true;
  }
  return false;
}

void countUse(const std::string &operand, std::map<std::string, int> &uses) {
  if (!operand.empty() && (operand[0] == '%' || operand[0] == '@')) {
    ++uses[operand];
  }
}

std::map<std::string, int> countValueUses(const Function &function) {
  std::map<std::string, int> uses;
  for (const std::string &line : function.instructions) {
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.empty()) {
      continue;
    }
    if (parts[0] == "ret") {
      if (parts.size() == 2) {
        countUse(parts[1], uses);
      }
      continue;
    }
    if (parts[0] == "br") {
      countUse(parts[1], uses);
      continue;
    }
    if (parts[0] == "jump") {
      continue;
    }
    if (parts[0] == "store") {
      if (parts.size() == 3) {
        countUse(parts[1], uses);
        countUse(parts[2], uses);
      }
      continue;
    }
    if (startsWith(line, "call @") ||
        (parts.size() >= 3 && parts[1] == "=" && parts[2] == "call")) {
      CallInstruction call = parseCallInstruction(line);
      for (const std::string &arg : call.args) {
        countUse(arg, uses);
      }
      continue;
    }
    if (parts.size() >= 3 && parts[1] == "=") {
      const std::string &op = parts[2];
      if (op == "load" && parts.size() == 4) {
        countUse(parts[3], uses);
      } else if ((op == "getelemptr" || op == "getptr") &&
                 parts.size() == 5) {
        countUse(parts[3], uses);
        countUse(parts[4], uses);
      } else if (parts.size() == 5) {
        countUse(parts[3], uses);
        countUse(parts[4], uses);
      }
    }
  }
  return uses;
}

std::string asmLabel(const std::string &function_name,
                     const std::string &koopa_label) {
  return function_name + "_" + stripSigil(koopa_label);
}

std::string loadBranchOperand(const std::string &operand,
                              const std::string &scratch,
                              const StackFrame &frame,
                              OperandEmitter &operands) {
  if (isLiteralValue(operand, "0")) {
    return "zero";
  }
  if (frame.hasRegisterValue(operand)) {
    return frame.registerFor(operand);
  }
  operands.loadOperand(operand, scratch);
  return scratch;
}

void emitDirectComparisonBranch(const std::string &function_name,
                                const std::string &comparison_line,
                                const std::string &then_label,
                                const std::string &else_label,
                                const StackFrame &frame,
                                OperandEmitter &operands,
                                AssemblyEmitter &output) {
  std::vector<std::string> parts = splitWhitespace(comparison_line);
  if (parts.size() != 5 || parts[1] != "=" || !isComparison(parts[2])) {
    return;
  }

  const std::string &op = parts[2];
  std::string left_reg = loadBranchOperand(parts[3], "t0", frame, operands);
  std::string right_reg = loadBranchOperand(parts[4], "t1", frame, operands);
  if (op == "eq") {
    output.instruction("beq " + left_reg + ", " + right_reg + ", " +
                       asmLabel(function_name, then_label));
  } else if (op == "ne") {
    output.instruction("bne " + left_reg + ", " + right_reg + ", " +
                       asmLabel(function_name, then_label));
  } else if (op == "lt") {
    output.instruction("blt " + left_reg + ", " + right_reg + ", " +
                       asmLabel(function_name, then_label));
  } else if (op == "gt") {
    output.instruction("blt " + right_reg + ", " + left_reg + ", " +
                       asmLabel(function_name, then_label));
  } else if (op == "le") {
    output.instruction("bge " + right_reg + ", " + left_reg + ", " +
                       asmLabel(function_name, then_label));
  } else if (op == "ge") {
    output.instruction("bge " + left_reg + ", " + right_reg + ", " +
                       asmLabel(function_name, then_label));
  }
  output.instruction("j " + asmLabel(function_name, else_label));
}

} // namespace

FunctionEmitter::FunctionEmitter(
    Function function, std::map<std::string, std::vector<int>> global_dimensions,
    RegisterAllocation registers)
    : function_(std::move(function)),
      frame_(function_, std::move(global_dimensions), std::move(registers)) {}

void FunctionEmitter::emit(std::ostream &output) {
  AssemblyEmitter asm_output(output);
  asm_output.sectionText();
  asm_output.global(function_.name);
  asm_output.label(function_.name);
  asm_output.adjustStack(-frame_.frameSize());
  if (frame_.hasCall()) {
    asm_output.storeWord("ra", frame_.raOffset());
  }
  for (const auto &[reg, offset] : frame_.calleeSavedRegisterOffsets()) {
    asm_output.storeWord(reg, offset);
  }

  OperandEmitter operands(frame_, asm_output);
  for (size_t i = 0; i < function_.params.size(); ++i) {
    if (i < 8) {
      operands.storeValue("a" + std::to_string(i), function_.params[i]);
    } else {
      asm_output.loadWord("t0",
                          frame_.frameSize() +
                              static_cast<int>((i - 8) * 4));
      operands.storeValue("t0", function_.params[i]);
    }
  }

  InstructionEmitter instructions(function_.name, frame_, asm_output);
  std::map<std::string, int> value_uses = countValueUses(function_);
  for (size_t i = 0; i < function_.instructions.size(); ++i) {
    if (i + 2 < function_.instructions.size()) {
      ComparisonAssignment comparison =
          parseComparisonAssignment(function_.instructions[i]);
      std::string bool_result;
      bool invert = false;
      BranchInstruction branch =
          parseBranchInstruction(function_.instructions[i + 2]);
      auto comparison_uses = value_uses.find(comparison.result);
      if (comparison.valid &&
          parseBooleanComparison(function_.instructions[i + 1],
                                 comparison.result, bool_result, invert) &&
          branch.valid && branch.condition == bool_result &&
          comparison_uses != value_uses.end() && comparison_uses->second == 1) {
        auto bool_uses = value_uses.find(bool_result);
        if (bool_uses != value_uses.end() && bool_uses->second == 1) {
          const std::string &then_label =
              invert ? branch.else_label : branch.then_label;
          const std::string &else_label =
              invert ? branch.then_label : branch.else_label;
          emitDirectComparisonBranch(function_.name, function_.instructions[i],
                                     then_label, else_label, frame_, operands,
                                     asm_output);
          i += 2;
          continue;
        }
      }
    }
    if (i + 1 < function_.instructions.size()) {
      ComparisonAssignment comparison =
          parseComparisonAssignment(function_.instructions[i]);
      BranchInstruction branch =
          parseBranchInstruction(function_.instructions[i + 1]);
      auto comparison_uses = value_uses.find(comparison.result);
      if (comparison.valid && branch.valid &&
          branch.condition == comparison.result &&
          comparison_uses != value_uses.end() && comparison_uses->second == 1) {
        emitDirectComparisonBranch(function_.name, function_.instructions[i],
                                   branch.then_label, branch.else_label,
                                   frame_, operands, asm_output);
        i += 1;
        continue;
      }
    }
    instructions.emitInstruction(function_.instructions[i], i);
  }
}

} // namespace compiler::riscv
