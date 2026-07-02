#include "compiler/ir/opt/analysis/liveness_analysis.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <algorithm>

namespace compiler::ir::opt {
namespace {

const int kBitsPerWord = 64;
using BitVector = std::vector<unsigned long long>;

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

void setBit(BitVector &bits, int id) {
  bits[static_cast<size_t>(id / kBitsPerWord)] |=
      1ULL << (id % kBitsPerWord);
}

void clearBit(BitVector &bits, int id) {
  bits[static_cast<size_t>(id / kBitsPerWord)] &=
      ~(1ULL << (id % kBitsPerWord));
}

bool getBit(const BitVector &bits, int id) {
  return (bits[static_cast<size_t>(id / kBitsPerWord)] &
          (1ULL << (id % kBitsPerWord))) != 0;
}

void orInto(BitVector &target, const BitVector &source) {
  for (size_t i = 0; i < target.size(); ++i) {
    target[i] |= source[i];
  }
}

std::set<std::string> bitsToSet(const BitVector &bits,
                                const std::vector<std::string> &values) {
  std::set<std::string> result;
  for (size_t word = 0; word < bits.size(); ++word) {
    unsigned long long remaining = bits[word];
    while (remaining != 0) {
      int bit = __builtin_ctzll(remaining);
      size_t id = word * kBitsPerWord + static_cast<size_t>(bit);
      if (id < values.size()) {
        result.insert(values[id]);
      }
      remaining &= remaining - 1;
    }
  }
  return result;
}

} // namespace

DenseLivenessInfo analyzeDenseLiveness(const IrFunction &function,
                                       const ControlFlowGraph &cfg) {
  const size_t count = function.instructions.size();
  std::vector<std::set<std::string>> uses(count);
  std::vector<std::set<std::string>> defs(count);
  DenseLivenessInfo info;
  auto internValue = [&](const std::string &value) {
    auto found = info.value_ids.find(value);
    if (found != info.value_ids.end()) {
      return found->second;
    }
    int id = static_cast<int>(info.values.size());
    info.value_ids[value] = id;
    info.values.push_back(value);
    return id;
  };

  for (size_t i = 0; i < count; ++i) {
    uses[i] = collectInstructionUses(function.instructions[i]);
    defs[i] = collectInstructionDefs(function.instructions[i]);
    for (const std::string &value : uses[i]) {
      internValue(value);
    }
    for (const std::string &value : defs[i]) {
      internValue(value);
    }
  }

  size_t word_count =
      (info.values.size() + static_cast<size_t>(kBitsPerWord - 1)) /
      static_cast<size_t>(kBitsPerWord);
  info.live_in.assign(count, BitVector(word_count, 0));
  info.live_out.assign(count, BitVector(word_count, 0));
  BitVector next_live(word_count, 0);
  BitVector new_out(word_count, 0);
  BitVector new_in(word_count, 0);

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto block_it = cfg.blocks.rbegin(); block_it != cfg.blocks.rend();
         ++block_it) {
      std::fill(next_live.begin(), next_live.end(), 0);
      for (size_t successor : block_it->successors) {
        const BasicBlock &successor_block = cfg.blocks[successor];
        if (successor_block.begin < successor_block.end) {
          orInto(next_live, info.live_in[successor_block.begin]);
        }
      }

      for (size_t i = block_it->end; i > block_it->begin; --i) {
        const size_t instruction = i - 1;
        new_out = next_live;
        new_in = new_out;
        for (const std::string &def : defs[instruction]) {
          clearBit(new_in, info.value_ids[def]);
        }
        for (const std::string &use : uses[instruction]) {
          setBit(new_in, info.value_ids[use]);
        }

        if (new_out != info.live_out[instruction] ||
            new_in != info.live_in[instruction]) {
          changed = true;
          info.live_out[instruction] = new_out;
          info.live_in[instruction] = new_in;
        }
        next_live = info.live_in[instruction];
      }
    }
  }

  return info;
}

DenseLivenessInfo analyzeDenseLiveness(const IrFunction &function) {
  return analyzeDenseLiveness(function, buildControlFlowGraph(function));
}

bool isLiveOut(const DenseLivenessInfo &info, size_t instruction,
               const std::string &value) {
  auto found = info.value_ids.find(value);
  if (found == info.value_ids.end() || instruction >= info.live_out.size()) {
    return false;
  }
  return getBit(info.live_out[instruction], found->second);
}

LivenessInfo analyzeLiveness(const IrFunction &function,
                             const ControlFlowGraph &cfg) {
  DenseLivenessInfo dense = analyzeDenseLiveness(function, cfg);
  LivenessInfo info;
  info.live_in.resize(function.instructions.size());
  info.live_out.resize(function.instructions.size());
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    info.live_in[i] = bitsToSet(dense.live_in[i], dense.values);
    info.live_out[i] = bitsToSet(dense.live_out[i], dense.values);
  }
  return info;
}

LivenessInfo analyzeLiveness(const IrFunction &function) {
  return analyzeLiveness(function, buildControlFlowGraph(function));
}

} // namespace compiler::ir::opt
