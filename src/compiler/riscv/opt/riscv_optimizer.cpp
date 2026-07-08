#include "compiler/riscv/opt/riscv_optimizer.h"

#include "compiler/riscv/opt/assembly_optimizer.h"

namespace compiler::riscv {

ProgramRegisterAllocation
RiscvOptimizer::allocateRegisters(const Program &program) const {
  return ProgramRegisterAllocator().allocate(program);
}

std::string
RiscvOptimizer::optimizeAssembly(const std::string &assembly) const {
  return AssemblyOptimizer().optimize(assembly);
}

} // namespace compiler::riscv
