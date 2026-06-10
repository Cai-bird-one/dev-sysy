#pragma once

#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/frame/stack_frame.h"

#include <string>

namespace compiler::riscv {

class OperandEmitter {
public:
  OperandEmitter(const StackFrame &frame, AssemblyEmitter &output);

  void emitPointerAddress(const std::string &pointer, const std::string &reg);
  void loadOperand(const std::string &operand, const std::string &reg);
  void loadFromPointer(const std::string &pointer, const std::string &reg);
  void storeToPointer(const std::string &reg, const std::string &pointer);
  void storeValue(const std::string &reg, const std::string &value);

private:
  const StackFrame &frame_;
  AssemblyEmitter &output_;
};

} // namespace compiler::riscv
