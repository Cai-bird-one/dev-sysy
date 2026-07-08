#include "compiler/riscv/riscv_generator.h"

#include "compiler/riscv/emit/assembly_emitter.h"
#include "compiler/riscv/emit/function_emitter.h"
#include "compiler/riscv/model/koopa_program.h"
#include "compiler/riscv/opt/riscv_optimizer.h"

#include <algorithm>
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
  bool emitted_data = false;
  for (const GlobalVariable &global : globals) {
    bool all_zero =
        std::all_of(global.initial_values.begin(), global.initial_values.end(),
                    [](int value) { return value == 0; });
    if (all_zero) {
      continue;
    }
    if (!emitted_data) {
      asm_output.sectionData();
      emitted_data = true;
    }
    asm_output.global(global.name);
    asm_output.label(global.name);
    for (int value : global.initial_values) {
      asm_output.word(value);
    }
  }

  bool emitted_bss = false;
  for (const GlobalVariable &global : globals) {
    bool all_zero =
        std::all_of(global.initial_values.begin(), global.initial_values.end(),
                    [](int value) { return value == 0; });
    if (!all_zero) {
      continue;
    }
    if (!emitted_bss) {
      asm_output.sectionBss();
      emitted_bss = true;
    }
    asm_output.global(global.name);
    asm_output.label(global.name);
    asm_output.zero(static_cast<int>(global.initial_values.size() * 4));
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

void emitProgram(Program program, const ProgramRegisterAllocation &registers,
                 std::ostream &output) {
  emitGlobals(program.globals, output);

  std::map<std::string, std::vector<int>> global_dimensions =
      buildGlobalDimensions(program.globals);
  for (Function &function : program.functions) {
    std::string function_name = function.name;
    FunctionEmitter emitter(std::move(function), global_dimensions,
                            registers.allocationFor(function_name));
    emitter.emit(output);
  }
}

} // namespace

std::string RiscvGenerator::generate(const std::string &koopa_ir) const {
  std::ostringstream output;
  generate(koopa_ir, output);
  return output.str();
}

std::string
RiscvGenerator::generateOptimized(const std::string &koopa_ir) const {
  Program program = parseProgram(koopa_ir);
  RiscvOptimizer optimizer;
  ProgramRegisterAllocation registers = optimizer.allocateRegisters(program);

  std::ostringstream assembly;
  emitProgram(std::move(program), registers, assembly);
  return optimizer.optimizeAssembly(assembly.str());
}

void RiscvGenerator::generate(const std::string &koopa_ir,
                              std::ostream &output) const {
  Program program = parseProgram(koopa_ir);
  RiscvOptimizer optimizer;
  ProgramRegisterAllocation registers = optimizer.allocateRegisters(program);
  emitProgram(std::move(program), registers, output);
}

void RiscvGenerator::generateOptimized(const std::string &koopa_ir,
                                       std::ostream &output) const {
  output << generateOptimized(koopa_ir);
}

} // namespace compiler::riscv
