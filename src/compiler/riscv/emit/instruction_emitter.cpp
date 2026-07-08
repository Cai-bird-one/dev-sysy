#include "compiler/riscv/emit/instruction_emitter.h"

#include "compiler/riscv/emit/operand_emitter.h"
#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

#include <utility>
#include <vector>

namespace compiler::riscv {
namespace {

bool parseImmediate12(const std::string &text, int &value) {
  if (!isIntegerLiteral(text)) {
    return false;
  }
  try {
    value = std::stoi(text);
  } catch (...) {
    return false;
  }
  return value >= -2048 && value <= 2047;
}

bool negatedImmediateFits12(int value, int &negated) {
  if (value == -2048) {
    return false;
  }
  negated = -value;
  return negated >= -2048 && negated <= 2047;
}

bool powerOfTwoShift(int value, int &shift) {
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

bool parseInteger(const std::string &text, long long &value) {
  if (!isIntegerLiteral(text)) {
    return false;
  }
  try {
    value = std::stoll(text);
  } catch (...) {
    return false;
  }
  return true;
}

bool isZeroLiteral(const std::string &text) {
  long long value = 0;
  return parseInteger(text, value) && value == 0;
}

bool powerOfTwoShift(long long value, int &shift) {
  if (value <= 0 || (value & (value - 1)) != 0) {
    return false;
  }
  shift = 0;
  while (value > 1) {
    value >>= 1;
    ++shift;
  }
  return shift <= 31;
}

bool fitsImmediate12(long long value) {
  return value >= -2048 && value <= 2047;
}

bool operandInRegister(const StackFrame &frame, const std::string &operand,
                       const std::string &reg) {
  return frame.hasRegisterValue(operand) && frame.registerFor(operand) == reg;
}

void loadBinaryOperands(const StackFrame &frame, OperandEmitter &operands,
                        const std::string &left, const std::string &right,
                        const std::string &target,
                        const std::string &right_reg) {
  if (left != right && operandInRegister(frame, right, target)) {
    operands.loadOperand(right, right_reg);
    operands.loadOperand(left, target);
    return;
  }
  operands.loadOperand(left, target);
  operands.loadOperand(right, right_reg);
}

} // namespace

InstructionEmitter::InstructionEmitter(std::string function_name,
                                       const StackFrame &frame,
                                       AssemblyEmitter &output)
    : function_name_(std::move(function_name)), frame_(frame),
      output_(output) {}

void InstructionEmitter::emitInstruction(const std::string &line,
                                         size_t instruction_index) {
  std::vector<std::string> parts = splitWhitespace(line);
  OperandEmitter operands(frame_, output_);
  if (parts.empty()) {
    return;
  }

  if (line.back() == ':') {
    std::string label = line.substr(0, line.size() - 1);
    if (label == "%entry") {
      return;
    }
    output_.label(asmLabel(label));
    return;
  }

  if (parts[0] == "br") {
    if (parts.size() != 4) {
      throw RiscvError("invalid branch instruction: " + line);
    }
    long long constant_condition = 0;
    if (parseInteger(parts[1], constant_condition)) {
      output_.instruction("j " + asmLabel(constant_condition != 0 ? parts[2]
                                                                  : parts[3]));
      return;
    }
    std::string condition_reg = "t0";
    if (frame_.hasRegisterValue(parts[1])) {
      condition_reg = frame_.registerFor(parts[1]);
    } else {
      operands.loadOperand(parts[1], condition_reg);
    }
    output_.instruction("bnez " + condition_reg + ", " + asmLabel(parts[2]));
    output_.instruction("j " + asmLabel(parts[3]));
    return;
  }

  if (parts[0] == "jump") {
    if (parts.size() != 2) {
      throw RiscvError("invalid jump instruction: " + line);
    }
    output_.instruction("j " + asmLabel(parts[1]));
    return;
  }

  if (parts[0] == "store") {
    if (parts.size() != 3) {
      throw RiscvError("invalid store instruction: " + line);
    }
    if (isZeroLiteral(parts[1])) {
      operands.storeToPointer("zero", parts[2]);
      return;
    }
    if (frame_.hasRegisterValue(parts[1])) {
      operands.storeToPointer(frame_.registerFor(parts[1]), parts[2]);
      return;
    }
    operands.loadOperand(parts[1], "t0");
    operands.storeToPointer("t0", parts[2]);
    return;
  }

  if (parts[0] == "ret") {
    if (parts.size() > 2) {
      throw RiscvError("invalid return instruction: " + line);
    }
    if (parts.size() == 2) {
      operands.loadOperand(parts[1], "a0");
    }
    if (frame_.hasCall()) {
      output_.loadWord("ra", frame_.raOffset());
    }
    for (const auto &[reg, offset] : frame_.calleeSavedRegisterOffsets()) {
      output_.loadWord(reg, offset);
    }
    output_.adjustStack(frame_.frameSize());
    output_.instruction("ret");
    return;
  }

  if (startsWith(line, "call @") ||
      (parts.size() >= 3 && parts[1] == "=" && parts[2] == "call")) {
    emitCall(parseCallInstruction(line), instruction_index);
    return;
  }

  if (parts.size() >= 3 && parts[1] == "=") {
    const std::string &result = parts[0];
    const std::string &op = parts[2];
    if (op == "alloc") {
      if (parts.size() < 4) {
        throw RiscvError("unsupported alloc instruction: " + line);
      }
      return;
    }
    if (op == "getelemptr") {
      if (parts.size() != 5) {
        throw RiscvError("invalid getelemptr instruction: " + line);
      }
      emitGetElementPtr(result, parts[3], parts[4], false);
      return;
    }
    if (op == "getptr") {
      if (parts.size() != 5) {
        throw RiscvError("invalid getptr instruction: " + line);
      }
      emitGetElementPtr(result, parts[3], parts[4], true);
      return;
    }
    if (op == "load") {
      if (parts.size() != 4) {
        throw RiscvError("invalid load instruction: " + line);
      }
      if (frame_.hasRegisterValue(result)) {
        operands.loadFromPointer(parts[3], frame_.registerFor(result));
        return;
      }
      operands.loadFromPointer(parts[3], "t0");
      operands.storeValue("t0", result);
      return;
    }
    if (parts.size() == 5) {
      const std::string target =
          frame_.hasRegisterValue(result) ? frame_.registerFor(result) : "t0";
      emitBinary(op, parts[3], parts[4], target);
      if (!frame_.hasRegisterValue(result)) {
        operands.storeValue(target, result);
      }
      return;
    }
  }

  throw RiscvError("unsupported Koopa instruction: " + line);
}

void InstructionEmitter::emitBinary(const std::string &op,
                                    const std::string &left,
                                    const std::string &right,
                                    const std::string &target) {
  OperandEmitter operands(frame_, output_);
  if (isComparison(op)) {
    if (isZeroLiteral(right)) {
      operands.loadOperand(left, target);
      if (op == "eq") {
        output_.instruction("seqz " + target + ", " + target);
      } else if (op == "ne") {
        output_.instruction("snez " + target + ", " + target);
      } else if (op == "lt") {
        output_.instruction("slt " + target + ", " + target + ", zero");
      } else if (op == "gt") {
        output_.instruction("slt " + target + ", zero, " + target);
      } else if (op == "le") {
        output_.instruction("slt " + target + ", zero, " + target);
        output_.instruction("xori " + target + ", " + target + ", 1");
      } else if (op == "ge") {
        output_.instruction("slt " + target + ", " + target + ", zero");
        output_.instruction("xori " + target + ", " + target + ", 1");
      }
      return;
    }
    if (isZeroLiteral(left)) {
      operands.loadOperand(right, target);
      if (op == "eq") {
        output_.instruction("seqz " + target + ", " + target);
      } else if (op == "ne") {
        output_.instruction("snez " + target + ", " + target);
      } else if (op == "lt") {
        output_.instruction("slt " + target + ", zero, " + target);
      } else if (op == "gt") {
        output_.instruction("slt " + target + ", " + target + ", zero");
      } else if (op == "le") {
        output_.instruction("slt " + target + ", " + target + ", zero");
        output_.instruction("xori " + target + ", " + target + ", 1");
      } else if (op == "ge") {
        output_.instruction("slt " + target + ", zero, " + target);
        output_.instruction("xori " + target + ", " + target + ", 1");
      }
      return;
    }
    long long immediate = 0;
    if (parseInteger(right, immediate)) {
      int imm12 = static_cast<int>(immediate);
      if ((op == "eq" || op == "ne") && fitsImmediate12(-immediate)) {
        operands.loadOperand(left, target);
        output_.instruction("addi " + target + ", " + target + ", " +
                            std::to_string(-immediate));
        output_.instruction((op == "eq" ? "seqz" : "snez") +
                            std::string(" ") + target + ", " + target);
        return;
      }
      if ((op == "lt" || op == "ge") && fitsImmediate12(immediate)) {
        operands.loadOperand(left, target);
        output_.instruction("slti " + target + ", " + target + ", " +
                            std::to_string(imm12));
        if (op == "ge") {
          output_.instruction("xori " + target + ", " + target + ", 1");
        }
        return;
      }
      if ((op == "le" || op == "gt") && fitsImmediate12(immediate + 1)) {
        operands.loadOperand(left, target);
        output_.instruction("slti " + target + ", " + target + ", " +
                            std::to_string(immediate + 1));
        if (op == "gt") {
          output_.instruction("xori " + target + ", " + target + ", 1");
        }
        return;
      }
    }
    if (parseInteger(left, immediate)) {
      if ((op == "eq" || op == "ne") && fitsImmediate12(-immediate)) {
        operands.loadOperand(right, target);
        output_.instruction("addi " + target + ", " + target + ", " +
                            std::to_string(-immediate));
        output_.instruction((op == "eq" ? "seqz" : "snez") +
                            std::string(" ") + target + ", " + target);
        return;
      }
      if ((op == "gt" || op == "le") && fitsImmediate12(immediate)) {
        operands.loadOperand(right, target);
        output_.instruction("slti " + target + ", " + target + ", " +
                            std::to_string(immediate));
        if (op == "le") {
          output_.instruction("xori " + target + ", " + target + ", 1");
        }
        return;
      }
      if ((op == "ge" || op == "lt") && fitsImmediate12(immediate + 1)) {
        operands.loadOperand(right, target);
        output_.instruction("slti " + target + ", " + target + ", " +
                            std::to_string(immediate + 1));
        if (op == "lt") {
          output_.instruction("xori " + target + ", " + target + ", 1");
        }
        return;
      }
    }
    loadBinaryOperands(frame_, operands, left, right, target, "t1");
    emitComparison(op, target, "t1", target);
    return;
  }
  int immediate = 0;
  if (op == "add" && parseImmediate12(right, immediate)) {
    operands.loadOperand(left, target);
    output_.instruction("addi " + target + ", " + target + ", " +
                        std::to_string(immediate));
    return;
  }
  if (op == "add" && parseImmediate12(left, immediate)) {
    operands.loadOperand(right, target);
    output_.instruction("addi " + target + ", " + target + ", " +
                        std::to_string(immediate));
    return;
  }
  if (op == "sub" && parseImmediate12(right, immediate)) {
    int negated = 0;
    if (negatedImmediateFits12(immediate, negated)) {
      operands.loadOperand(left, target);
      output_.instruction("addi " + target + ", " + target + ", " +
                          std::to_string(negated));
      return;
    }
  }
  if ((op == "and" || op == "or") && parseImmediate12(right, immediate)) {
    operands.loadOperand(left, target);
    output_.instruction(op + "i " + target + ", " + target + ", " +
                        std::to_string(immediate));
    return;
  }
  if ((op == "and" || op == "or") && parseImmediate12(left, immediate)) {
    operands.loadOperand(right, target);
    output_.instruction(op + "i " + target + ", " + target + ", " +
                        std::to_string(immediate));
    return;
  }
  if (op == "mul") {
    long long constant = 0;
    std::string operand;
    if (parseInteger(right, constant)) {
      operand = left;
    } else if (parseInteger(left, constant)) {
      operand = right;
    }

    if (!operand.empty()) {
      if (constant == 0) {
        output_.loadImmediate(target, "0");
        return;
      }
      operands.loadOperand(operand, target);
      if (constant == 1) {
        return;
      }
      int shift = 0;
      if (powerOfTwoShift(constant, shift)) {
        output_.instruction("slli " + target + ", " + target + ", " +
                            std::to_string(shift));
        return;
      }
      if (constant > 1 && powerOfTwoShift(constant - 1, shift)) {
        output_.instruction("slli t1, " + target + ", " +
                            std::to_string(shift));
        output_.instruction("add " + target + ", t1, " + target);
        return;
      }
      if (constant > 1 && powerOfTwoShift(constant + 1, shift)) {
        output_.instruction("slli t1, " + target + ", " +
                            std::to_string(shift));
        output_.instruction("sub " + target + ", t1, " + target);
        return;
      }
    }
  }
  if (op == "div" || op == "mod") {
    long long divisor = 0;
    if (parseInteger(right, divisor)) {
      int shift = 0;
      if (powerOfTwoShift(divisor, shift)) {
        if (op == "mod" && shift == 0) {
          output_.loadImmediate(target, "0");
          return;
        }
        operands.loadOperand(left, target);
        if (shift == 0) {
          return;
        }

        long long bias = divisor - 1;
        output_.instruction("srai t1, " + target + ", 31");
        if (fitsImmediate12(bias)) {
          output_.instruction("andi t1, t1, " + std::to_string(bias));
        } else {
          output_.loadImmediate("t2", std::to_string(bias));
          output_.instruction("and t1, t1, t2");
        }
        output_.instruction("add t1, " + target + ", t1");
        output_.instruction("srai t1, t1, " + std::to_string(shift));
        if (op == "div") {
          if (target != "t1") {
            output_.instruction("mv " + target + ", t1");
          }
        } else {
          output_.instruction("slli t1, t1, " + std::to_string(shift));
          output_.instruction("sub " + target + ", " + target + ", t1");
        }
        return;
      }
    }
  }
  loadBinaryOperands(frame_, operands, left, right, target, "t1");
  output_.instruction(koopaBinaryToRiscv(op) + " " + target + ", " + target +
                      ", t1");
}

void InstructionEmitter::emitCall(const CallInstruction &call,
                                  size_t instruction_index) {
  OperandEmitter operands(frame_, output_);
  const std::set<std::string> &saved_values =
      frame_.callSavedValues(instruction_index);
  saveCallerSavedValues(saved_values);
  for (size_t i = 0; i < call.args.size(); ++i) {
    if (i < 8) {
      operands.loadOperand(call.args[i], "a" + std::to_string(i));
    } else {
      operands.loadOperand(call.args[i], "t0");
      output_.storeWord("t0", static_cast<int>((i - 8) * 4));
    }
  }
  output_.instruction("call " + stripSigil(call.callee));
  restoreCallerSavedValues(saved_values);
  if (call.has_result) {
    operands.storeValue("a0", call.result);
  }
}

void InstructionEmitter::saveCallerSavedValues(
    const std::set<std::string> &values) {
  for (const std::string &value : values) {
    output_.storeWord(frame_.registerFor(value), frame_.offsetOf(value));
  }
}

void InstructionEmitter::restoreCallerSavedValues(
    const std::set<std::string> &values) {
  for (const std::string &value : values) {
    output_.loadWord(frame_.registerFor(value), frame_.offsetOf(value));
  }
}

void InstructionEmitter::emitComparison(const std::string &op,
                                        const std::string &left_reg,
                                        const std::string &right_reg,
                                        const std::string &target) {
  if (op == "lt") {
    output_.instruction("slt " + target + ", " + left_reg + ", " +
                        right_reg);
    return;
  }
  if (op == "gt") {
    output_.instruction("slt " + target + ", " + right_reg + ", " +
                        left_reg);
    return;
  }
  if (op == "le") {
    output_.instruction("slt " + target + ", " + right_reg + ", " +
                        left_reg);
    output_.instruction("xori " + target + ", " + target + ", 1");
    return;
  }
  if (op == "ge") {
    output_.instruction("slt " + target + ", " + left_reg + ", " +
                        right_reg);
    output_.instruction("xori " + target + ", " + target + ", 1");
    return;
  }
  if (op == "eq") {
    output_.instruction("sub " + target + ", " + left_reg + ", " +
                        right_reg);
    output_.instruction("seqz " + target + ", " + target);
    return;
  }
  if (op == "ne") {
    output_.instruction("sub " + target + ", " + left_reg + ", " +
                        right_reg);
    output_.instruction("snez " + target + ", " + target);
    return;
  }
}

void InstructionEmitter::emitGetElementPtr(const std::string &result,
                                           const std::string &base,
                                           const std::string &index,
                                           bool is_getptr) {
  OperandEmitter operands(frame_, output_);
  operands.emitPointerAddress(base, "t0");
  int stride = 4;
  std::vector<int> dimensions = frame_.dimensionsForPointer(base);
  if (is_getptr && !dimensions.empty()) {
    stride = elementCount(dimensions) * 4;
  } else if (dimensions.size() > 1) {
    stride = elementCount(dimensions, 1) * 4;
  }
  long long constant_index = 0;
  if (parseInteger(index, constant_index)) {
    long long offset = constant_index * stride;
    if (offset != 0) {
      if (fitsImmediate12(offset)) {
        output_.instruction("addi t0, t0, " + std::to_string(offset));
      } else {
        output_.loadImmediate("t1", std::to_string(offset));
        output_.instruction("add t0, t0, t1");
      }
    }
    operands.storeValue("t0", result);
    return;
  }
  operands.loadOperand(index, "t1");
  int shift = 0;
  if (powerOfTwoShift(stride, shift)) {
    output_.instruction("slli t1, t1, " + std::to_string(shift));
  } else {
    output_.loadImmediate("t2", stride);
    output_.instruction("mul t1, t1, t2");
  }
  output_.instruction("add t0, t0, t1");
  operands.storeValue("t0", result);
}

std::string InstructionEmitter::asmLabel(const std::string &koopa_label) const {
  return function_name_ + "_" + stripSigil(koopa_label);
}

} // namespace compiler::riscv
