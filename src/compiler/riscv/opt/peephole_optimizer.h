#pragma once

#include <string>

namespace compiler::riscv {

class PeepholeOptimizer {
public:
  std::string optimize(const std::string &assembly) const;
};

} // namespace compiler::riscv
