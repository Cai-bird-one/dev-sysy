#pragma once

#include <string>

namespace compiler::riscv {

class AssemblyOptimizationPass {
public:
  virtual ~AssemblyOptimizationPass() = default;

  virtual const char *name() const = 0;
  virtual std::string run(const std::string &assembly) const = 0;
};

class AssemblyOptimizer {
public:
  std::string optimize(const std::string &assembly) const;
};

} // namespace compiler::riscv
