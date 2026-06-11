#pragma once

#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/frame/stack_frame.h"
#include "compiler/riscv/model/koopa_program.h"

#include <cstddef>
#include <set>
#include <string>

namespace compiler::riscv {

class InstructionEmitter {
public:
  InstructionEmitter(std::string function_name, const StackFrame &frame,
                     AssemblyEmitter &output);

  void emitInstruction(const std::string &line, size_t instruction_index);

private:
  void emitBinary(const std::string &op, const std::string &left,
                  const std::string &right);
  void emitCall(const CallInstruction &call, size_t instruction_index);
  void emitComparison(const std::string &op);
  void emitGetElementPtr(const std::string &result, const std::string &base,
                         const std::string &index, bool is_getptr);
  void saveCallerSavedValues(const std::set<std::string> &values);
  void restoreCallerSavedValues(const std::set<std::string> &values);
  std::string asmLabel(const std::string &koopa_label) const;

  std::string function_name_;
  const StackFrame &frame_;
  AssemblyEmitter &output_;
};

} // namespace compiler::riscv
