#pragma once

#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/emit/instruction_emitter.h"
#include "compiler/riscv/emit/operand_emitter.h"
#include "compiler/riscv/frame/stack_frame.h"
#include "compiler/riscv/model/koopa_program.h"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace compiler::riscv {

class FunctionEmitter {
public:
  FunctionEmitter(Function function,
                  std::map<std::string, std::vector<int>> global_dimensions);

  void emit(std::ostream &output);

private:
  Function function_;
  StackFrame frame_;
};

} // namespace compiler::riscv
