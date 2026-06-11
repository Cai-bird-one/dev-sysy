#include "compiler/ir/opt/passes/global_value_facts.h"

#include <set>

namespace compiler::ir::opt {
namespace {

bool isBinaryOp(const std::string &op) {
  return op == "add" || op == "sub" || op == "mul" || op == "div" ||
         op == "mod" || op == "eq" || op == "ne" || op == "lt" ||
         op == "gt" || op == "le" || op == "ge";
}

bool isTrackableResult(const std::string &name) {
  return startsWith(name, "%");
}

bool foldedBinaryValue(const Assignment &assignment, std::string &value) {
  if (assignment.args.size() != 2 || !isBinaryOp(assignment.op) ||
      !isInteger(assignment.args[0]) || !isInteger(assignment.args[1]) ||
      (assignment.op == "div" && assignment.args[1] == "0") ||
      (assignment.op == "mod" && assignment.args[1] == "0")) {
    return false;
  }
  value = std::to_string(evaluateBinary(
      assignment.op, std::stoll(assignment.args[0]),
      std::stoll(assignment.args[1])));
  return true;
}

bool simplifiedCopyValue(const Assignment &assignment, std::string &value) {
  if (assignment.args.size() != 2) {
    return false;
  }

  const std::string &lhs = assignment.args[0];
  const std::string &rhs = assignment.args[1];
  if ((assignment.op == "add" && rhs == "0") ||
      (assignment.op == "sub" && rhs == "0") ||
      (assignment.op == "mul" && rhs == "1") ||
      (assignment.op == "div" && rhs == "1")) {
    value = lhs;
    return true;
  }
  if ((assignment.op == "add" && lhs == "0") ||
      (assignment.op == "mul" && lhs == "1")) {
    value = rhs;
    return true;
  }
  if (assignment.op == "mul" && (lhs == "0" || rhs == "0")) {
    value = "0";
    return true;
  }
  return false;
}

ValueFacts transferBlock(const IrFunction &function, const BasicBlock &block,
                         ValueFacts facts) {
  for (size_t i = block.begin; i < block.end; ++i) {
    if (isLabel(function.instructions[i])) {
      continue;
    }
    Assignment assignment = parseAssignment(function.instructions[i]);
    if (assignment.valid) {
      assignment = assignmentWithResolvedValueFacts(assignment, facts);
      updateValueFactsForAssignment(assignment, facts);
    }
  }
  return facts;
}

ValueFacts meetFacts(const std::vector<size_t> &predecessors,
                     const std::vector<ValueFacts> &block_out) {
  if (predecessors.empty()) {
    return {};
  }

  ValueFacts result = block_out[predecessors[0]];
  for (size_t i = 1; i < predecessors.size(); ++i) {
    const ValueFacts &incoming = block_out[predecessors[i]];
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

} // namespace

std::string resolveValueFact(const std::string &value,
                             const ValueFacts &facts) {
  std::set<std::string> seen;
  std::string current = value;
  while (seen.insert(current).second) {
    auto found = facts.find(current);
    if (found == facts.end()) {
      break;
    }
    current = found->second;
  }
  return current;
}

std::vector<std::pair<std::string, std::string>>
valueFactReplacements(const ValueFacts &facts) {
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto &fact : facts) {
    std::string resolved = resolveValueFact(fact.second, facts);
    if (resolved != fact.first) {
      result.push_back({fact.first, resolved});
    }
  }
  return result;
}

Assignment assignmentWithResolvedValueFacts(Assignment assignment,
                                           const ValueFacts &facts) {
  for (std::string &arg : assignment.args) {
    arg = resolveValueFact(arg, facts);
  }
  return assignment;
}

void updateValueFactsForAssignment(const Assignment &assignment,
                                   ValueFacts &facts) {
  if (!isTrackableResult(assignment.result)) {
    return;
  }

  std::string value;
  if (foldedBinaryValue(assignment, value) ||
      simplifiedCopyValue(assignment, value)) {
    value = resolveValueFact(value, facts);
    if (value != assignment.result) {
      facts[assignment.result] = value;
      return;
    }
  }
  facts.erase(assignment.result);
}

std::vector<ValueFacts>
computeBlockEntryValueFacts(const IrFunction &function,
                            const ControlFlowGraph &cfg) {
  std::vector<ValueFacts> block_in(cfg.blocks.size());
  std::vector<ValueFacts> block_out(cfg.blocks.size());

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < cfg.blocks.size(); ++i) {
      ValueFacts incoming =
          i == 0 ? ValueFacts{} : meetFacts(cfg.blocks[i].predecessors,
                                            block_out);
      ValueFacts outgoing = transferBlock(function, cfg.blocks[i], incoming);
      if (incoming != block_in[i] || outgoing != block_out[i]) {
        changed = true;
        block_in[i] = std::move(incoming);
        block_out[i] = std::move(outgoing);
      }
    }
  }
  return block_in;
}

} // namespace compiler::ir::opt
