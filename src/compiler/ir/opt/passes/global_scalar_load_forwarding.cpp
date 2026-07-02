#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/analysis/control_flow_graph.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <utility>

namespace compiler::ir::opt {
namespace {

using MemoryFacts = std::map<std::string, std::string>;

std::set<std::string> collectScalarAllocs(const IrFunction &function) {
  std::set<std::string> allocs;
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && assignment.op == "alloc" &&
        assignment.args.size() == 1 && assignment.args[0] == "i32") {
      allocs.insert(assignment.result);
    }
  }
  return allocs;
}

bool isScalarPointer(const std::string &value,
                     const std::set<std::string> &scalar_allocs) {
  return scalar_allocs.find(value) != scalar_allocs.end();
}

bool isCall(const Assignment &assignment, const std::vector<std::string> &parts) {
  return (assignment.valid && assignment.op == "call") ||
         (!parts.empty() && parts[0] == "call");
}

MemoryFacts meetFacts(const std::vector<size_t> &predecessors,
                      const std::vector<MemoryFacts> &block_out) {
  if (predecessors.empty()) {
    return {};
  }

  MemoryFacts result = block_out[predecessors[0]];
  for (size_t i = 1; i < predecessors.size(); ++i) {
    const MemoryFacts &incoming = block_out[predecessors[i]];
    for (auto it = result.begin(); it != result.end();) {
      auto found = incoming.find(it->first);
      if (found == incoming.end() || found->second != it->second) {
        it = result.erase(it);
      } else {
        ++it;
      }
    }
  }
  return result;
}

MemoryFacts transferBlock(const IrFunction &function, const BasicBlock &block,
                          const std::set<std::string> &scalar_allocs,
                          MemoryFacts facts) {
  for (size_t i = block.begin; i < block.end; ++i) {
    const std::string &line = function.instructions[i];
    if (isLabel(line)) {
      continue;
    }

    std::vector<std::string> parts = splitWhitespace(line);
    Assignment assignment = parseAssignment(line);
    if (isCall(assignment, parts)) {
      facts.clear();
      continue;
    }

    if (parts.size() == 3 && parts[0] == "store") {
      if (isScalarPointer(parts[2], scalar_allocs)) {
        facts[parts[2]] = parts[1];
      } else {
        facts.clear();
      }
    }
  }
  return facts;
}

std::vector<MemoryFacts>
computeBlockEntryFacts(const IrFunction &function, const ControlFlowGraph &cfg,
                       const std::set<std::string> &scalar_allocs) {
  std::vector<MemoryFacts> block_in(cfg.blocks.size());
  std::vector<MemoryFacts> block_out(cfg.blocks.size());

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < cfg.blocks.size(); ++i) {
      MemoryFacts incoming =
          i == 0 ? MemoryFacts{} : meetFacts(cfg.blocks[i].predecessors,
                                             block_out);
      MemoryFacts outgoing =
          transferBlock(function, cfg.blocks[i], scalar_allocs, incoming);
      if (incoming != block_in[i] || outgoing != block_out[i]) {
        changed = true;
        block_in[i] = std::move(incoming);
        block_out[i] = std::move(outgoing);
      }
    }
  }

  return block_in;
}

std::string copyValueInstruction(const std::string &result,
                                 const std::string &value) {
  Assignment copy;
  copy.valid = true;
  copy.result = result;
  copy.op = "add";
  copy.args = {value, "0"};
  return formatAssignment(copy);
}

} // namespace

PassResult GlobalScalarLoadForwardingPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> scalar_allocs = collectScalarAllocs(function);
  if (scalar_allocs.empty()) {
    return result;
  }

  ControlFlowGraph cfg = buildControlFlowGraph(function);
  std::vector<MemoryFacts> block_in =
      computeBlockEntryFacts(function, cfg, scalar_allocs);
  std::vector<std::string> optimized;

  for (size_t block_index = 0; block_index < cfg.blocks.size();
       ++block_index) {
    MemoryFacts facts = block_in[block_index];
    const BasicBlock &block = cfg.blocks[block_index];
    for (size_t i = block.begin; i < block.end; ++i) {
      const std::string &line = function.instructions[i];
      if (isLabel(line)) {
        optimized.push_back(line);
        continue;
      }

      std::vector<std::string> parts = splitWhitespace(line);
      Assignment assignment = parseAssignment(line);
      if (isCall(assignment, parts)) {
        facts.clear();
        optimized.push_back(line);
        continue;
      }

      if (parts.size() == 3 && parts[0] == "store") {
        if (isScalarPointer(parts[2], scalar_allocs)) {
          facts[parts[2]] = parts[1];
        } else {
          facts.clear();
        }
        optimized.push_back(line);
        continue;
      }

      if (assignment.valid && assignment.op == "load" &&
          assignment.args.size() == 1 &&
          isScalarPointer(assignment.args[0], scalar_allocs)) {
        auto found = facts.find(assignment.args[0]);
        if (found != facts.end()) {
          optimized.push_back(copyValueInstruction(assignment.result,
                                                   found->second));
          result.changed = true;
          continue;
        }
      }

      optimized.push_back(line);
    }
  }

  function.instructions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
