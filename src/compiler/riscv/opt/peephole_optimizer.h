#pragma once

#include "compiler/riscv/opt/assembly_optimizer.h"

#include <string>

namespace compiler::riscv {

class PeepholeOptimizer : public AssemblyOptimizationPass {
public:
  const char *name() const override;
  std::string run(const std::string &assembly) const override;
  std::string optimize(const std::string &assembly) const;
};

} // namespace compiler::riscv
