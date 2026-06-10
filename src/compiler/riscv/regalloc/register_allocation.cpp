#include "compiler/riscv/regalloc/register_allocation.h"

#include "compiler/riscv/riscv_generator.h"

#include <utility>

namespace compiler::riscv {

bool RegisterAllocation::hasRegister(const std::string &value) const {
  return registers_.find(value) != registers_.end();
}

const std::string &
RegisterAllocation::registerFor(const std::string &value) const {
  auto found = registers_.find(value);
  if (found == registers_.end()) {
    throw RiscvError("unknown allocated Koopa value: " + value);
  }
  return found->second;
}

void RegisterAllocation::assign(const std::string &value, std::string reg) {
  registers_[value] = std::move(reg);
}

} // namespace compiler::riscv
