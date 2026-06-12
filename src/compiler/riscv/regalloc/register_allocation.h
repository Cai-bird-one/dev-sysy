#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>

namespace compiler::riscv {

class RegisterAllocation {
public:
  bool hasRegister(const std::string &value) const;
  const std::string &registerFor(const std::string &value) const;
  std::set<std::string> usedRegisters() const;
  void assign(const std::string &value, std::string reg);
  void addCallSavedValue(size_t instruction_index, const std::string &value);
  const std::set<std::string> &callSavedValues(size_t instruction_index) const;
  bool needsCallSaveSlot(const std::string &value) const;

private:
  std::map<std::string, std::string> registers_;
  std::map<size_t, std::set<std::string>> call_saved_values_;
  std::set<std::string> call_save_slots_;
};

} // namespace compiler::riscv
