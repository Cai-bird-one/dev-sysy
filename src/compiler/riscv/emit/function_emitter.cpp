#include "compiler/riscv/emit/function_emitter.h"

#include <ostream>
#include <utility>

namespace compiler::riscv {

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
  for (const std::string &line : function_.instructions) {
    instructions.emitInstruction(line);
  }
}

} // namespace compiler::riscv
