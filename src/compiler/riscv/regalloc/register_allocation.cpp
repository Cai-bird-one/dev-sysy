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

void RegisterAllocation::addCallSavedValue(size_t instruction_index,
                                           const std::string &value) {
  call_saved_values_[instruction_index].insert(value);
  call_save_slots_.insert(value);
}

const std::set<std::string> &
RegisterAllocation::callSavedValues(size_t instruction_index) const {
  static const std::set<std::string> empty;
  auto found = call_saved_values_.find(instruction_index);
  if (found == call_saved_values_.end()) {
    return empty;
  }
  return found->second;
}

bool RegisterAllocation::needsCallSaveSlot(const std::string &value) const {
  return call_save_slots_.find(value) != call_save_slots_.end();
}

} // namespace compiler::riscv
