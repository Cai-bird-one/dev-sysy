#include "compiler/riscv/emit/operand_emitter.h"

#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

namespace compiler::riscv {

OperandEmitter::OperandEmitter(const StackFrame &frame, AssemblyEmitter &output)
    : frame_(frame), output_(output) {}

void OperandEmitter::emitPointerAddress(const std::string &pointer,
                                        const std::string &reg) {
  if (frame_.isPointer(pointer) && frame_.hasStackValue(pointer)) {
    if (frame_.isAggregateAlloc(pointer)) {
      output_.loadStackAddress(frame_.offsetOf(pointer), reg);
    } else {
      output_.loadWord(reg, frame_.offsetOf(pointer));
    }
    return;
  }
  if (startsWith(pointer, "@")) {
    output_.loadAddress(reg, stripSigil(pointer));
    return;
  }
  throw RiscvError("unknown Koopa pointer: " + pointer);
}

void OperandEmitter::loadOperand(const std::string &operand,
                                 const std::string &reg) {
  if (isIntegerLiteral(operand)) {
    output_.loadImmediate(reg, operand);
    return;
  }
  if (frame_.hasRegisterValue(operand)) {
    const std::string &allocated = frame_.registerFor(operand);
    if (allocated != reg) {
      output_.instruction("mv " + reg + ", " + allocated);
    }
    return;
  }
  if (frame_.hasStackValue(operand)) {
    output_.loadWord(reg, frame_.offsetOf(operand));
    return;
  }
  if (startsWith(operand, "@")) {
    output_.loadAddress(reg, stripSigil(operand));
    output_.loadWord(reg, 0, reg);
    return;
  }
  throw RiscvError("unknown Koopa value: " + operand);
}

void OperandEmitter::loadFromPointer(const std::string &pointer,
                                     const std::string &reg) {
  if (startsWith(pointer, "@")) {
    output_.loadAddress(reg, stripSigil(pointer));
    output_.loadWord(reg, 0, reg);
    return;
  }
  if (!frame_.hasStackValue(pointer)) {
    throw RiscvError("unknown Koopa pointer: " + pointer);
  }
  if (frame_.isPointer(pointer) && frame_.stackSizeOf(pointer) == 4) {
    output_.loadWord(reg, frame_.offsetOf(pointer));
    output_.loadWord(reg, 0, reg);
    return;
  }
  output_.loadWord(reg, frame_.offsetOf(pointer));
}

void OperandEmitter::storeToPointer(const std::string &reg,
                                    const std::string &pointer) {
  if (startsWith(pointer, "@")) {
    output_.loadAddress("t1", stripSigil(pointer));
    output_.storeWord(reg, 0, "t1");
    return;
  }
  if (frame_.hasStackValue(pointer) && frame_.isPointer(pointer) &&
      frame_.stackSizeOf(pointer) == 4) {
    output_.loadWord("t1", frame_.offsetOf(pointer));
    output_.storeWord(reg, 0, "t1");
    return;
  }
  storeValue(reg, pointer);
}

void OperandEmitter::storeValue(const std::string &reg,
                                const std::string &value) {
  if (frame_.hasRegisterValue(value)) {
    const std::string &allocated = frame_.registerFor(value);
    if (allocated != reg) {
      output_.instruction("mv " + allocated + ", " + reg);
    }
    return;
  }
  output_.storeWord(reg, frame_.offsetOf(value));
}

} // namespace compiler::riscv
