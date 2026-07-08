#include "compiler/riscv/opt/assembly_optimizer.h"

#include "compiler/riscv/opt/peephole_optimizer.h"

#include <memory>
#include <utility>
#include <vector>

namespace compiler::riscv {
namespace {

constexpr int kMaxOptimizationIterations = 128;

std::vector<std::unique_ptr<AssemblyOptimizationPass>> buildPasses() {
  std::vector<std::unique_ptr<AssemblyOptimizationPass>> passes;
  passes.push_back(std::make_unique<PeepholeOptimizer>());
  return passes;
}

std::string
runPassesOnce(const std::string &assembly,
              const std::vector<std::unique_ptr<AssemblyOptimizationPass>>
                  &passes) {
  std::string current = assembly;
  for (const auto &pass : passes) {
    current = pass->run(current);
  }
  return current;
}

} // namespace

std::string AssemblyOptimizer::optimize(const std::string &assembly) const {
  std::vector<std::unique_ptr<AssemblyOptimizationPass>> passes = buildPasses();
  std::string current = assembly;
  for (int i = 0; i < kMaxOptimizationIterations; ++i) {
    std::string next = runPassesOnce(current, passes);
    if (next == current) {
      break;
    }
    current = std::move(next);
  }
  return current;
}

} // namespace compiler::riscv
