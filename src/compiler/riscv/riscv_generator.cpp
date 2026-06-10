#include "compiler/riscv/riscv_generator.h"

#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/emit/function_emitter.h"
#include "compiler/riscv/model/koopa_program.h"
#include "compiler/riscv/opt/peephole_optimizer.h"

#include <map>
#include <ostream>
#include <sstream>
#include <utility>
#include <vector>

namespace compiler::riscv {
namespace {

void emitGlobals(const std::vector<GlobalVariable> &globals,
                 std::ostream &output) {
  if (globals.empty()) {
    return;
  }

  AssemblyEmitter asm_output(output);
  asm_output.sectionData();
  for (const GlobalVariable &global : globals) {
    asm_output.global(global.name);
    asm_output.label(global.name);
    for (int value : global.initial_values) {
      asm_output.word(value);
    }
  }
}

std::map<std::string, std::vector<int>>
buildGlobalDimensions(const std::vector<GlobalVariable> &globals) {
  std::map<std::string, std::vector<int>> global_dimensions;
  for (const GlobalVariable &global : globals) {
    global_dimensions[global.name] = global.dimensions;
  }
  return global_dimensions;
}

} // namespace

std::string RiscvGenerator::generate(const std::string &koopa_ir) const {
  std::ostringstream output;
  generate(koopa_ir, output);
  return output.str();
}

std::string
RiscvGenerator::generateOptimized(const std::string &koopa_ir) const {
  PeepholeOptimizer optimizer;
  return optimizer.optimize(generate(koopa_ir));
}

void RiscvGenerator::generate(const std::string &koopa_ir,
                              std::ostream &output) const {
  Program program = parseProgram(koopa_ir);

  emitGlobals(program.globals, output);

  std::map<std::string, std::vector<int>> global_dimensions =
      buildGlobalDimensions(program.globals);
  for (Function &function : program.functions) {
    FunctionEmitter emitter(std::move(function), global_dimensions);
    emitter.emit(output);
  }
}

void RiscvGenerator::generateOptimized(const std::string &koopa_ir,
                                       std::ostream &output) const {
  output << generateOptimized(koopa_ir);
}

} // namespace compiler::riscv
