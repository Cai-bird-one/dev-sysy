#pragma once

#include "compiler/riscv/model/koopa_program.h"
#include "compiler/riscv/regalloc/register_allocation.h"

#include <map>
#include <string>

namespace compiler::riscv {

class ProgramRegisterAllocation {
public:
  void assignFunction(std::string function_name, RegisterAllocation allocation);
  const RegisterAllocation &allocationFor(const std::string &function_name) const;

private:
  std::map<std::string, RegisterAllocation> allocations_;
};

class ProgramRegisterAllocator {
public:
  ProgramRegisterAllocation allocate(const Program &program) const;
};

} // namespace compiler::riscv
