#pragma once

#include <map>
#include <string>

namespace compiler::riscv {

class RegisterAllocation {
public:
  bool hasRegister(const std::string &value) const;
  const std::string &registerFor(const std::string &value) const;
  void assign(const std::string &value, std::string reg);

private:
  std::map<std::string, std::string> registers_;
};

} // namespace compiler::riscv
