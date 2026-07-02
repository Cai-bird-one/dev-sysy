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
    operands.loadOperand(parts[1], "t0");
    output_.instruction("bnez t0, " + asmLabel(parts[2]));
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
      operands.loadFromPointer(parts[3], "t0");
      operands.storeValue("t0", result);
      return;
    }
    if (parts.size() == 5) {
      emitBinary(op, parts[3], parts[4]);
      operands.storeValue("t0", result);
      return;
    }
  }

  throw RiscvError("unsupported Koopa instruction: " + line);
}

void InstructionEmitter::emitBinary(const std::string &op,
                                    const std::string &left,
                                    const std::string &right) {
  OperandEmitter operands(frame_, output_);
  if (isComparison(op)) {
    if (isZeroLiteral(right)) {
      operands.loadOperand(left, "t0");
      if (op == "eq") {
        output_.instruction("seqz t0, t0");
      } else if (op == "ne") {
        output_.instruction("snez t0, t0");
      } else if (op == "lt") {
        output_.instruction("slt t0, t0, zero");
      } else if (op == "gt") {
        output_.instruction("slt t0, zero, t0");
      } else if (op == "le") {
        output_.instruction("slt t0, zero, t0");
        output_.instruction("xori t0, t0, 1");
      } else if (op == "ge") {
        output_.instruction("slt t0, t0, zero");
        output_.instruction("xori t0, t0, 1");
      }
      return;
    }
    if (isZeroLiteral(left)) {
      operands.loadOperand(right, "t0");
      if (op == "eq") {
        output_.instruction("seqz t0, t0");
      } else if (op == "ne") {
        output_.instruction("snez t0, t0");
      } else if (op == "lt") {
        output_.instruction("slt t0, zero, t0");
      } else if (op == "gt") {
        output_.instruction("slt t0, t0, zero");
      } else if (op == "le") {
        output_.instruction("slt t0, t0, zero");
        output_.instruction("xori t0, t0, 1");
      } else if (op == "ge") {
        output_.instruction("slt t0, zero, t0");
        output_.instruction("xori t0, t0, 1");
      }
      return;
    }
    operands.loadOperand(left, "t0");
    operands.loadOperand(right, "t1");
    emitComparison(op);
    return;
  }
  int immediate = 0;
  if (op == "add" && parseImmediate12(right, immediate)) {
    operands.loadOperand(left, "t0");
    output_.instruction("addi t0, t0, " + std::to_string(immediate));
    return;
  }
  if (op == "add" && parseImmediate12(left, immediate)) {
    operands.loadOperand(right, "t0");
    output_.instruction("addi t0, t0, " + std::to_string(immediate));
    return;
  }
  if (op == "sub" && parseImmediate12(right, immediate)) {
    int negated = 0;
    if (negatedImmediateFits12(immediate, negated)) {
      operands.loadOperand(left, "t0");
      output_.instruction("addi t0, t0, " + std::to_string(negated));
      return;
    }
  }
  if ((op == "and" || op == "or") && parseImmediate12(right, immediate)) {
    operands.loadOperand(left, "t0");
    output_.instruction(op + "i t0, t0, " + std::to_string(immediate));
    return;
  }
  if ((op == "and" || op == "or") && parseImmediate12(left, immediate)) {
    operands.loadOperand(right, "t0");
    output_.instruction(op + "i t0, t0, " + std::to_string(immediate));
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
        output_.loadImmediate("t0", "0");
        return;
      }
      operands.loadOperand(operand, "t0");
      if (constant == 1) {
        return;
      }
      int shift = 0;
      if (powerOfTwoShift(constant, shift)) {
        output_.instruction("slli t0, t0, " + std::to_string(shift));
        return;
      }
    }
  }
  operands.loadOperand(left, "t0");
  operands.loadOperand(right, "t1");
  output_.instruction(koopaBinaryToRiscv(op) + " t0, t0, t1");
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

void InstructionEmitter::emitComparison(const std::string &op) {
  if (op == "lt") {
    output_.instruction("slt t0, t0, t1");
    return;
  }
  if (op == "gt") {
    output_.instruction("slt t0, t1, t0");
    return;
  }
  if (op == "le") {
    output_.instruction("slt t0, t1, t0");
    output_.instruction("xori t0, t0, 1");
    return;
  }
  if (op == "ge") {
    output_.instruction("slt t0, t0, t1");
    output_.instruction("xori t0, t0, 1");
    return;
  }
  if (op == "eq") {
    output_.instruction("sub t0, t0, t1");
    output_.instruction("seqz t0, t0");
    return;
  }
  if (op == "ne") {
    output_.instruction("sub t0, t0, t1");
    output_.instruction("snez t0, t0");
    return;
  }
}

void InstructionEmitter::emitGetElementPtr(const std::string &result,
                                           const std::string &base,
                                           const std::string &index,
                                           bool is_getptr) {
  OperandEmitter operands(frame_, output_);
  operands.emitPointerAddress(base, "t0");
  operands.loadOperand(index, "t1");
  int stride = 4;
  std::vector<int> dimensions = frame_.dimensionsForPointer(base);
  if (is_getptr && !dimensions.empty()) {
    stride = elementCount(dimensions) * 4;
  } else if (dimensions.size() > 1) {
    stride = elementCount(dimensions, 1) * 4;
  }
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
