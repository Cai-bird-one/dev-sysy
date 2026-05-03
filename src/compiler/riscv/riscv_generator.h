#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>

namespace compiler::riscv {

class RiscvError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class RiscvGenerator {
public:
  std::string generate(const std::string &koopa_ir) const;
  void generate(const std::string &koopa_ir, std::ostream &output) const;

private:
  struct ReturnFunction {
    std::string name;
    std::string value;
  };

  ReturnFunction parseReturnFunction(const std::string &koopa_ir) const;
};

} // namespace compiler::riscv
