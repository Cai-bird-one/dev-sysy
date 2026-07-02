#include "compiler/riscv/regalloc/register_allocator.h"

namespace compiler::riscv {

RegisterAllocation RegisterAllocator::allocate(const Function &function) const {
  std::set<std::string> allocatable = collectAllocatableValues(function);
  std::set<std::string> register_params = collectRegisterParamValues(function);
  std::vector<InstructionInfo> instructions =
      analyzeInstructions(function, allocatable);
  LivenessInfo liveness = analyzeLiveness(function);
  RegisterAllocation allocation =
      linearScanAllocate(buildLiveIntervals(function, allocatable,
                                            register_params, instructions,
                                            liveness));
  recordCallSavedValues(function, liveness, allocation);
  return allocation;
}

} // namespace compiler::riscv
