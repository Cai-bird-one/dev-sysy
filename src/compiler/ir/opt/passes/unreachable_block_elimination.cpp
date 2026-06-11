#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <vector>

namespace compiler::ir::opt {
namespace {

struct BlockRange {
  size_t begin = 0;
  size_t end = 0;
  std::string label;
};

std::string labelName(const std::string &line) {
  return line.substr(0, line.size() - 1);
}

std::vector<BlockRange>
buildBlocks(const std::vector<std::string> &instructions) {
  std::vector<BlockRange> blocks;
  for (size_t i = 0; i < instructions.size();) {
    size_t begin = i;
    std::string label;
    if (isLabel(instructions[i])) {
      label = labelName(instructions[i]);
      ++i;
    }
    while (i < instructions.size() && !isLabel(instructions[i])) {
      ++i;
    }
    blocks.push_back({begin, i, label});
  }
  return blocks;
}

std::map<std::string, size_t>
buildLabelIndex(const std::vector<BlockRange> &blocks) {
  std::map<std::string, size_t> labels;
  for (size_t i = 0; i < blocks.size(); ++i) {
    if (!blocks[i].label.empty()) {
      labels[blocks[i].label] = i;
    }
  }
  return labels;
}

std::vector<size_t>
successors(size_t index, const std::vector<BlockRange> &blocks,
           const std::vector<std::string> &instructions,
           const std::map<std::string, size_t> &labels) {
  if (blocks[index].begin == blocks[index].end) {
    return {};
  }

  const std::string &last = instructions[blocks[index].end - 1];
  std::vector<std::string> parts = splitWhitespace(last);
  std::vector<size_t> result;
  if (parts.size() == 2 && parts[0] == "jump") {
    auto found = labels.find(parts[1]);
    if (found != labels.end()) {
      result.push_back(found->second);
    }
    return result;
  }
  if (parts.size() == 4 && parts[0] == "br") {
    auto true_target = labels.find(parts[2]);
    if (true_target != labels.end()) {
      result.push_back(true_target->second);
    }
    auto false_target = labels.find(parts[3]);
    if (false_target != labels.end() &&
        (true_target == labels.end() ||
         false_target->second != true_target->second)) {
      result.push_back(false_target->second);
    }
    return result;
  }
  if (!parts.empty() && parts[0] == "ret") {
    return result;
  }
  if (index + 1 < blocks.size()) {
    result.push_back(index + 1);
  }
  return result;
}

std::set<size_t> reachableBlocks(const std::vector<BlockRange> &blocks,
                                 const std::vector<std::string> &instructions) {
  std::set<size_t> reachable;
  if (blocks.empty()) {
    return reachable;
  }

  std::map<std::string, size_t> labels = buildLabelIndex(blocks);
  std::vector<size_t> stack = {0};
  while (!stack.empty()) {
    size_t current = stack.back();
    stack.pop_back();
    if (!reachable.insert(current).second) {
      continue;
    }
    for (size_t next : successors(current, blocks, instructions, labels)) {
      stack.push_back(next);
    }
  }
  return reachable;
}

} // namespace

PassResult UnreachableBlockEliminationPass::run(IrFunction &function) {
  PassResult result;
  std::vector<BlockRange> blocks = buildBlocks(function.instructions);
  std::set<size_t> reachable =
      reachableBlocks(blocks, function.instructions);
  if (reachable.size() == blocks.size()) {
    return result;
  }

  std::vector<std::string> optimized;
  for (size_t i = 0; i < blocks.size(); ++i) {
    if (reachable.find(i) == reachable.end()) {
      result.changed = true;
      continue;
    }
    optimized.insert(optimized.end(),
                     function.instructions.begin() + blocks[i].begin,
                     function.instructions.begin() + blocks[i].end);
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
