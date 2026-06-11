#include "compiler/ir/opt/analysis/control_flow_graph.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>

namespace compiler::ir::opt {
namespace {

std::string labelName(const std::string &line) {
  return line.substr(0, line.size() - 1);
}

std::vector<BasicBlock> splitBlocks(const IrFunction &function) {
  std::vector<BasicBlock> blocks;
  const std::vector<std::string> &instructions = function.instructions;
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
    blocks.push_back({begin, i, label, {}, {}});
  }
  return blocks;
}

std::map<std::string, size_t>
buildLabelIndex(const std::vector<BasicBlock> &blocks) {
  std::map<std::string, size_t> labels;
  for (size_t i = 0; i < blocks.size(); ++i) {
    if (!blocks[i].label.empty()) {
      labels[blocks[i].label] = i;
    }
  }
  return labels;
}

void addSuccessor(std::vector<size_t> &successors, size_t target) {
  for (size_t successor : successors) {
    if (successor == target) {
      return;
    }
  }
  successors.push_back(target);
}

void fillSuccessors(ControlFlowGraph &cfg,
                    const std::vector<std::string> &instructions) {
  std::map<std::string, size_t> labels = buildLabelIndex(cfg.blocks);
  for (size_t i = 0; i < cfg.blocks.size(); ++i) {
    BasicBlock &block = cfg.blocks[i];
    if (block.begin == block.end) {
      continue;
    }

    const std::string &last = instructions[block.end - 1];
    std::vector<std::string> parts = splitWhitespace(last);
    if (parts.size() == 2 && parts[0] == "jump") {
      auto found = labels.find(parts[1]);
      if (found != labels.end()) {
        addSuccessor(block.successors, found->second);
      }
    } else if (parts.size() == 4 && parts[0] == "br") {
      auto true_target = labels.find(parts[2]);
      if (true_target != labels.end()) {
        addSuccessor(block.successors, true_target->second);
      }
      auto false_target = labels.find(parts[3]);
      if (false_target != labels.end()) {
        addSuccessor(block.successors, false_target->second);
      }
    } else if (parts.empty() || parts[0] != "ret") {
      if (i + 1 < cfg.blocks.size()) {
        addSuccessor(block.successors, i + 1);
      }
    }
  }
}

void fillPredecessors(ControlFlowGraph &cfg) {
  for (size_t i = 0; i < cfg.blocks.size(); ++i) {
    for (size_t successor : cfg.blocks[i].successors) {
      cfg.blocks[successor].predecessors.push_back(i);
    }
  }
}

std::vector<size_t> buildInstructionBlocks(const ControlFlowGraph &cfg,
                                           size_t instruction_count) {
  std::vector<size_t> instruction_blocks(instruction_count, 0);
  for (size_t i = 0; i < cfg.blocks.size(); ++i) {
    for (size_t instruction = cfg.blocks[i].begin;
         instruction < cfg.blocks[i].end; ++instruction) {
      instruction_blocks[instruction] = i;
    }
  }
  return instruction_blocks;
}

} // namespace

ControlFlowGraph buildControlFlowGraph(const IrFunction &function) {
  ControlFlowGraph cfg;
  cfg.blocks = splitBlocks(function);
  fillSuccessors(cfg, function.instructions);
  fillPredecessors(cfg);
  cfg.instruction_blocks =
      buildInstructionBlocks(cfg, function.instructions.size());
  return cfg;
}

} // namespace compiler::ir::opt
