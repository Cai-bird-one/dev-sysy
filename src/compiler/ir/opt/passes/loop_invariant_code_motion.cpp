#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/analysis/control_flow_graph.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace compiler::ir::opt {
namespace {

struct NaturalLoop {
  size_t header = 0;
  std::set<size_t> blocks;
};

struct Preheader {
  bool valid = false;
  size_t insertion_index = 0;
};

std::set<size_t> allBlocks(size_t count) {
  std::set<size_t> result;
  for (size_t i = 0; i < count; ++i) {
    result.insert(i);
  }
  return result;
}

std::set<size_t> intersectSets(const std::set<size_t> &left,
                               const std::set<size_t> &right) {
  std::set<size_t> result;
  std::set_intersection(left.begin(), left.end(), right.begin(), right.end(),
                        std::inserter(result, result.begin()));
  return result;
}

std::vector<std::set<size_t>>
computeDominators(const ControlFlowGraph &cfg) {
  const size_t count = cfg.blocks.size();
  std::vector<std::set<size_t>> dominators(count);
  if (count == 0) {
    return dominators;
  }

  dominators[0].insert(0);
  std::set<size_t> universe = allBlocks(count);
  for (size_t i = 1; i < count; ++i) {
    dominators[i] = universe;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t block = 1; block < count; ++block) {
      std::set<size_t> next;
      const std::vector<size_t> &predecessors = cfg.blocks[block].predecessors;
      if (predecessors.empty()) {
        next.insert(block);
      } else {
        next = dominators[predecessors[0]];
        for (size_t i = 1; i < predecessors.size(); ++i) {
          next = intersectSets(next, dominators[predecessors[i]]);
        }
        next.insert(block);
      }

      if (next != dominators[block]) {
        dominators[block] = std::move(next);
        changed = true;
      }
    }
  }

  return dominators;
}

bool dominates(const std::vector<std::set<size_t>> &dominators,
               size_t dominator, size_t block) {
  return block < dominators.size() &&
         dominators[block].find(dominator) != dominators[block].end();
}

std::set<size_t> buildNaturalLoop(const ControlFlowGraph &cfg, size_t header,
                                  size_t latch) {
  std::set<size_t> blocks;
  std::vector<size_t> stack;
  blocks.insert(header);
  blocks.insert(latch);
  stack.push_back(latch);

  while (!stack.empty()) {
    size_t block = stack.back();
    stack.pop_back();
    for (size_t predecessor : cfg.blocks[block].predecessors) {
      if (blocks.insert(predecessor).second && predecessor != header) {
        stack.push_back(predecessor);
      }
    }
  }

  return blocks;
}

std::vector<NaturalLoop> findNaturalLoops(const ControlFlowGraph &cfg) {
  std::vector<std::set<size_t>> dominators = computeDominators(cfg);
  std::map<size_t, NaturalLoop> loops_by_header;

  for (size_t block = 0; block < cfg.blocks.size(); ++block) {
    for (size_t successor : cfg.blocks[block].successors) {
      if (!dominates(dominators, successor, block)) {
        continue;
      }

      NaturalLoop &loop = loops_by_header[successor];
      loop.header = successor;
      std::set<size_t> blocks = buildNaturalLoop(cfg, successor, block);
      loop.blocks.insert(blocks.begin(), blocks.end());
    }
  }

  std::vector<NaturalLoop> loops;
  for (auto &entry : loops_by_header) {
    loops.push_back(std::move(entry.second));
  }
  std::sort(loops.begin(), loops.end(),
            [](const NaturalLoop &left, const NaturalLoop &right) {
              if (left.blocks.size() != right.blocks.size()) {
                return left.blocks.size() > right.blocks.size();
              }
              return left.header < right.header;
            });
  return loops;
}

bool containsBlock(const NaturalLoop &loop, size_t block) {
  return loop.blocks.find(block) != loop.blocks.end();
}

Preheader findPreheader(const IrFunction &function, const ControlFlowGraph &cfg,
                        const NaturalLoop &loop) {
  std::vector<size_t> outside_predecessors;
  for (size_t predecessor : cfg.blocks[loop.header].predecessors) {
    if (!containsBlock(loop, predecessor)) {
      outside_predecessors.push_back(predecessor);
    }
  }
  if (outside_predecessors.size() != 1) {
    return {};
  }

  const size_t preheader = outside_predecessors[0];
  const BasicBlock &block = cfg.blocks[preheader];
  if (block.successors.size() != 1 || block.successors[0] != loop.header) {
    return {};
  }

  const BasicBlock &header = cfg.blocks[loop.header];
  if (block.begin == block.end) {
    return {};
  }

  std::vector<std::string> parts =
      splitWhitespace(function.instructions[block.end - 1]);
  if (parts.size() == 2 && parts[0] == "jump" && !header.label.empty() &&
      parts[1] == header.label) {
    return {true, block.end - 1};
  }
  if (block.end == header.begin &&
      !isTerminator(function.instructions[block.end - 1])) {
    return {true, block.end};
  }

  return {};
}

bool isHoistableOperation(const std::string &op) {
  return op == "add" || op == "sub" || op == "mul" || op == "eq" ||
         op == "ne" || op == "lt" || op == "gt" || op == "le" ||
         op == "ge" || op == "getelemptr" || op == "getptr";
}

bool isHoistableAssignment(const Assignment &assignment) {
  return assignment.valid && startsWith(assignment.result, "%") &&
         isHoistableOperation(assignment.op);
}

std::set<std::string> collectLoopDefinitions(const IrFunction &function,
                                             const ControlFlowGraph &cfg,
                                             const NaturalLoop &loop) {
  std::set<std::string> definitions;
  for (size_t block_index : loop.blocks) {
    const BasicBlock &block = cfg.blocks[block_index];
    for (size_t i = block.begin; i < block.end; ++i) {
      Assignment assignment = parseAssignment(function.instructions[i]);
      if (assignment.valid) {
        definitions.insert(assignment.result);
      }
    }
  }
  return definitions;
}

bool isInvariantOperand(const std::string &operand,
                        const std::set<std::string> &loop_definitions,
                        const std::set<std::string> &invariant_values) {
  if (!isValueName(operand)) {
    return true;
  }
  if (loop_definitions.find(operand) == loop_definitions.end()) {
    return true;
  }
  return invariant_values.find(operand) != invariant_values.end();
}

bool hasInvariantOperands(const Assignment &assignment,
                          const std::set<std::string> &loop_definitions,
                          const std::set<std::string> &invariant_values) {
  for (const std::string &operand : assignment.args) {
    if (!isInvariantOperand(operand, loop_definitions, invariant_values)) {
      return false;
    }
  }
  return true;
}

std::vector<size_t>
collectHoistableInstructions(const IrFunction &function,
                             const ControlFlowGraph &cfg,
                             const NaturalLoop &loop,
                             const std::set<size_t> &removed,
                             const std::set<std::string> &hoisted_values) {
  std::set<std::string> loop_definitions =
      collectLoopDefinitions(function, cfg, loop);
  std::set<std::string> invariant_values = hoisted_values;
  std::set<size_t> selected;
  std::vector<size_t> instructions;

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t block_index : loop.blocks) {
      const BasicBlock &block = cfg.blocks[block_index];
      for (size_t i = block.begin; i < block.end; ++i) {
        if (removed.find(i) != removed.end() ||
            selected.find(i) != selected.end()) {
          continue;
        }

        Assignment assignment = parseAssignment(function.instructions[i]);
        if (!isHoistableAssignment(assignment) ||
            !hasInvariantOperands(assignment, loop_definitions,
                                  invariant_values)) {
          continue;
        }

        selected.insert(i);
        instructions.push_back(i);
        invariant_values.insert(assignment.result);
        changed = true;
      }
    }
  }

  return instructions;
}

void insertBefore(std::map<size_t, std::vector<std::string>> &insertions,
                  size_t index, const std::string &line) {
  insertions[index].push_back(line);
}

} // namespace

PassResult LoopInvariantCodeMotionPass::run(IrFunction &function) {
  PassResult result;
  ControlFlowGraph cfg = buildControlFlowGraph(function);
  std::vector<NaturalLoop> loops = findNaturalLoops(cfg);
  if (loops.empty()) {
    return result;
  }

  std::map<size_t, std::vector<std::string>> insertions;
  std::set<size_t> removed;
  std::set<std::string> hoisted_values;

  for (const NaturalLoop &loop : loops) {
    Preheader preheader = findPreheader(function, cfg, loop);
    if (!preheader.valid) {
      continue;
    }

    std::vector<size_t> hoistable =
        collectHoistableInstructions(function, cfg, loop, removed,
                                     hoisted_values);
    for (size_t instruction : hoistable) {
      Assignment assignment =
          parseAssignment(function.instructions[instruction]);
      insertBefore(insertions, preheader.insertion_index,
                   function.instructions[instruction]);
      removed.insert(instruction);
      hoisted_values.insert(assignment.result);
      result.changed = true;
    }
  }

  if (!result.changed) {
    return result;
  }

  std::vector<std::string> optimized;
  for (size_t i = 0; i <= function.instructions.size(); ++i) {
    auto insertion = insertions.find(i);
    if (insertion != insertions.end()) {
      optimized.insert(optimized.end(), insertion->second.begin(),
                       insertion->second.end());
    }
    if (i == function.instructions.size()) {
      break;
    }
    if (removed.find(i) == removed.end()) {
      optimized.push_back(function.instructions[i]);
    }
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
