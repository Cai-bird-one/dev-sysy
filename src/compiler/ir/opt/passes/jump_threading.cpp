#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <utility>

namespace compiler::ir::opt {
namespace {

std::string labelName(const std::string &line) {
  return line.substr(0, line.size() - 1);
}

std::map<std::string, std::string>
collectTrivialJumps(const std::vector<std::string> &instructions) {
  std::map<std::string, std::string> jumps;
  for (size_t i = 0; i + 1 < instructions.size(); ++i) {
    if (!isLabel(instructions[i])) {
      continue;
    }
    std::vector<std::string> parts = splitWhitespace(instructions[i + 1]);
    if (parts.size() == 2 && parts[0] == "jump" &&
        parts[1] != labelName(instructions[i])) {
      jumps[labelName(instructions[i])] = parts[1];
    }
  }
  return jumps;
}

std::string resolveTarget(const std::string &target,
                          const std::map<std::string, std::string> &jumps) {
  std::string current = target;
  std::set<std::string> seen;
  while (seen.insert(current).second) {
    auto found = jumps.find(current);
    if (found == jumps.end()) {
      break;
    }
    current = found->second;
  }
  return current;
}

bool fallsThroughToLabel(const std::vector<std::string> &instructions,
                         size_t index, const std::string &target) {
  return index + 1 < instructions.size() && isLabel(instructions[index + 1]) &&
         labelName(instructions[index + 1]) == target;
}

} // namespace

PassResult JumpThreadingPass::run(IrFunction &function) {
  PassResult result;
  std::map<std::string, std::string> jumps =
      collectTrivialJumps(function.instructions);
  std::vector<std::string> optimized;

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const std::string &line = function.instructions[i];
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.size() == 2 && parts[0] == "jump") {
      std::string target = resolveTarget(parts[1], jumps);
      if (fallsThroughToLabel(function.instructions, i, target)) {
        result.changed = true;
        continue;
      }
      optimized.push_back("jump " + target);
      result.changed = result.changed || target != parts[1];
      continue;
    }
    if (parts.size() == 4 && parts[0] == "br") {
      std::string true_target = resolveTarget(parts[2], jumps);
      std::string false_target = resolveTarget(parts[3], jumps);
      optimized.push_back("br " + parts[1] + ", " + true_target + ", " +
                          false_target);
      result.changed = result.changed || true_target != parts[2] ||
                       false_target != parts[3];
      continue;
    }
    optimized.push_back(line);
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
