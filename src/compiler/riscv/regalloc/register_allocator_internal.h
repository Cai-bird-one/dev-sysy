#pragma once

#include "compiler/riscv/regalloc/register_allocator.h"
#include "compiler/riscv/util/riscv_utils.h"

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace compiler::riscv::regalloc_detail {

inline const std::vector<std::string> kTempRegisters = {"t3", "t4", "t5",
                                                        "t6"};
inline const std::vector<std::string> kArgumentRegisters = {
    "a1", "a2", "a3", "a4", "a5", "a6", "a7"};
inline const std::vector<std::string> kCalleeSavedRegisters = {
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};

inline constexpr int kNoFutureUse = std::numeric_limits<int>::max() / 4;
inline constexpr int kBitsPerWord = 64;

using BitVector = std::vector<unsigned long long>;

inline bool isKoopaValue(const std::string &text) {
  return !text.empty() && text[0] == '%';
}

inline bool isCalleeSavedRegister(const std::string &reg) {
  return std::find(kCalleeSavedRegisters.begin(), kCalleeSavedRegisters.end(),
                   reg) != kCalleeSavedRegisters.end();
}

inline bool isRegisterParam(const Function &function, size_t index) {
  if (index >= 8) {
    return false;
  }
  if (index >= function.param_types.size()) {
    return true;
  }
  return function.param_types[index] == "i32" ||
         startsWith(function.param_types[index], "*");
}

inline bool isCallInstruction(const std::string &line,
                              const std::vector<std::string> &parts) {
  return startsWith(line, "call @") ||
         (parts.size() >= 3 && parts[1] == "=" && parts[2] == "call");
}

inline void addUse(std::set<std::string> &uses,
                   const std::set<std::string> &allowed,
                   const std::string &value) {
  if (allowed.find(value) != allowed.end()) {
    uses.insert(value);
  }
}

inline void appendAll(std::vector<std::string> &target,
                      const std::vector<std::string> &source) {
  target.insert(target.end(), source.begin(), source.end());
}

inline bool containsRegister(const std::vector<std::string> &registers,
                             const std::string &reg) {
  return std::find(registers.begin(), registers.end(), reg) != registers.end();
}

inline void recordPoint(RegisterAllocator::LiveInterval &interval,
                        int position, bool is_use) {
  interval.start = std::min(interval.start, position);
  interval.end = std::max(interval.end, position);
  if (is_use) {
    interval.uses.push_back(position);
  }
}

template <typename Callback>
void forEachSetBit(const BitVector &bits, const std::vector<std::string> &values,
                   Callback callback) {
  for (size_t word = 0; word < bits.size(); ++word) {
    unsigned long long remaining = bits[word];
    while (remaining != 0) {
      int bit = __builtin_ctzll(remaining);
      size_t id = word * kBitsPerWord + static_cast<size_t>(bit);
      if (id < values.size()) {
        callback(values[id]);
      }
      remaining &= remaining - 1;
    }
  }
}

} // namespace compiler::riscv::regalloc_detail
