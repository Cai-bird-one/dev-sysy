#include "compiler/riscv/regalloc/program_register_allocator.h"

#include "compiler/riscv/regalloc/register_allocator.h"
#include "compiler/riscv/riscv_generator.h"

#include <utility>

namespace compiler::riscv {

void ProgramRegisterAllocation::assignFunction(
    std::string function_name, RegisterAllocation allocation) {
  allocations_[std::move(function_name)] = std::move(allocation);
}

const RegisterAllocation &
ProgramRegisterAllocation::allocationFor(const std::string &function_name) const {
  auto found = allocations_.find(function_name);
  if (found == allocations_.end()) {
    throw RiscvError("missing register allocation for function: " +
                     function_name);
  }
  return found->second;
}

ProgramRegisterAllocation
ProgramRegisterAllocator::allocate(const Program &program) const {
  ProgramRegisterAllocation program_allocation;
  RegisterAllocator function_allocator;
  for (const Function &function : program.functions) {
    program_allocation.assignFunction(function.name,
                                      function_allocator.allocate(function));
  }
  return program_allocation;
}

} // namespace compiler::riscv
