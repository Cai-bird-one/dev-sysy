#include "compiler/riscv/regalloc/register_allocator_internal.h"

#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

namespace compiler::riscv {

using namespace regalloc_detail;

void RegisterAllocator::recordCallSavedValues(
    const Function &function, const LivenessInfo &liveness,
    RegisterAllocation &allocation) const {
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const std::string &line = function.instructions[i];
    std::vector<std::string> parts = splitWhitespace(line);
    if (!isCallInstruction(line, parts)) {
      continue;
    }
    CallInstruction call = parseCallInstruction(line);
    forEachSetBit(liveness.live_out[i], liveness.values,
                  [&](const std::string &value) {
                    if (call.has_result && value == call.result) {
                      return;
                    }
                    if (allocation.hasRegister(value) &&
                        !isCalleeSavedRegister(allocation.registerFor(value))) {
                      allocation.addCallSavedValue(i, value);
                    }
                  });
  }
}

} // namespace compiler::riscv
