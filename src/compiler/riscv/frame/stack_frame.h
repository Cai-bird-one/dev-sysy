#pragma once

#include "compiler/riscv/model/koopa_program.h"

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::riscv {

class StackFrame {
public:
  StackFrame(const Function &function,
             std::map<std::string, std::vector<int>> global_dimensions);

  int frameSize() const;
  int raOffset() const;
  bool hasCall() const;

  bool hasStackValue(const std::string &value) const;
  int offsetOf(const std::string &value) const;
  int stackSizeOf(const std::string &value) const;

  bool isPointer(const std::string &value) const;
  bool isAggregateAlloc(const std::string &value) const;
  std::vector<int> dimensionsForPointer(const std::string &pointer) const;

private:
  void assignStackSlots();
  int localStackSizeOf(const std::string &value) const;

  const Function &function_;
  std::map<std::string, std::vector<int>> global_dimensions_;
  std::map<std::string, int> stack_offsets_;
  std::map<std::string, int> stack_sizes_;
  std::map<std::string, std::vector<int>> pointer_dimensions_;
  std::set<std::string> aggregate_allocs_;
  int frame_size_ = 0;
  int ra_offset_ = 0;
  int outgoing_arg_size_ = 0;
  size_t max_call_args_ = 0;
  bool has_call_ = false;
};

} // namespace compiler::riscv
