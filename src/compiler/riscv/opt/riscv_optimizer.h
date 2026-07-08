#pragma once

#include "compiler/riscv/model/koopa_program.h"
#include "compiler/riscv/regalloc/program_register_allocator.h"

#include <string>

namespace compiler::riscv {

class RiscvOptimizer {
public:
  ProgramRegisterAllocation allocateRegisters(const Program &program) const;
  std::string optimizeAssembly(const std::string &assembly) const;
};

} // namespace compiler::riscv
