#include "compiler/ir/opt/analysis/liveness_analysis.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

namespace compiler::ir::opt {
namespace {

std::set<std::string> collectBranchUses(const std::string &line) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() >= 2 && isValueName(parts[1])) {
    return {parts[1]};
  }
  return {};
}

std::set<std::string> collectInstructionUses(const std::string &line) {
  if (startsWith(line, "jump ")) {
    return {};
  }
  if (startsWith(line, "br ")) {
    return collectBranchUses(line);
  }

  std::set<std::string> uses;
  Assignment assignment = parseAssignment(line);
  for (const std::string &name : collectValueNames(line)) {
    if (!assignment.valid || name != assignment.result) {
      uses.insert(name);
    }
  }
  return uses;
}

std::set<std::string> collectInstructionDefs(const std::string &line) {
  Assignment assignment = parseAssignment(line);
  if (!assignment.valid) {
    return {};
  }
  return {assignment.result};
}

std::set<std::string> withoutDefs(std::set<std::string> values,
                                  const std::set<std::string> &defs) {
  for (const std::string &def : defs) {
    values.erase(def);
  }
  return values;
}

void addAll(std::set<std::string> &target, const std::set<std::string> &items) {
  target.insert(items.begin(), items.end());
}

} // namespace

LivenessInfo analyzeLiveness(const IrFunction &function,
                             const ControlFlowGraph &cfg) {
  const size_t count = function.instructions.size();
  std::vector<std::set<std::string>> uses(count);
  std::vector<std::set<std::string>> defs(count);
  for (size_t i = 0; i < count; ++i) {
    uses[i] = collectInstructionUses(function.instructions[i]);
    defs[i] = collectInstructionDefs(function.instructions[i]);
  }

  LivenessInfo info;
  info.live_in.resize(count);
  info.live_out.resize(count);

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto block_it = cfg.blocks.rbegin(); block_it != cfg.blocks.rend();
         ++block_it) {
      std::set<std::string> next_live;
      for (size_t successor : block_it->successors) {
        const BasicBlock &successor_block = cfg.blocks[successor];
        if (successor_block.begin < successor_block.end) {
          addAll(next_live, info.live_in[successor_block.begin]);
        }
      }

      for (size_t i = block_it->end; i > block_it->begin; --i) {
        const size_t instruction = i - 1;
        std::set<std::string> new_out = next_live;
        std::set<std::string> new_in = withoutDefs(new_out, defs[instruction]);
        addAll(new_in, uses[instruction]);

        if (new_out != info.live_out[instruction] ||
            new_in != info.live_in[instruction]) {
          changed = true;
          info.live_out[instruction] = std::move(new_out);
          info.live_in[instruction] = std::move(new_in);
        }
        next_live = info.live_in[instruction];
      }
    }
  }

  return info;
}

LivenessInfo analyzeLiveness(const IrFunction &function) {
  return analyzeLiveness(function, buildControlFlowGraph(function));
}

} // namespace compiler::ir::opt
