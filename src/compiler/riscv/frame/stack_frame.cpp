#include "compiler/riscv/frame/stack_frame.h"

#include "compiler/riscv/riscv_generator.h"
#include "compiler/riscv/util/riscv_utils.h"

#include <utility>

namespace compiler::riscv {

StackFrame::StackFrame(
    const Function &function,
    std::map<std::string, std::vector<int>> global_dimensions,
    RegisterAllocation registers)
    : function_(function), global_dimensions_(std::move(global_dimensions)),
      registers_(std::move(registers)) {
  assignStackSlots();
}

int StackFrame::frameSize() const { return frame_size_; }

int StackFrame::raOffset() const { return ra_offset_; }

bool StackFrame::hasCall() const { return has_call_; }

bool StackFrame::hasStackValue(const std::string &value) const {
  return stack_offsets_.find(value) != stack_offsets_.end();
}

int StackFrame::offsetOf(const std::string &value) const {
  auto found = stack_offsets_.find(value);
  if (found == stack_offsets_.end()) {
    throw RiscvError("unknown Koopa stack value: " + value);
  }
  return found->second;
}

int StackFrame::stackSizeOf(const std::string &value) const {
  return localStackSizeOf(value);
}

bool StackFrame::hasRegisterValue(const std::string &value) const {
  return registers_.hasRegister(value);
}

const std::string &StackFrame::registerFor(const std::string &value) const {
  return registers_.registerFor(value);
}

const std::set<std::string> &
StackFrame::callSavedValues(size_t instruction_index) const {
  return registers_.callSavedValues(instruction_index);
}

bool StackFrame::isPointer(const std::string &value) const {
  return pointer_dimensions_.find(value) != pointer_dimensions_.end();
}

bool StackFrame::isAggregateAlloc(const std::string &value) const {
  return aggregate_allocs_.find(value) != aggregate_allocs_.end();
}

std::vector<int>
StackFrame::dimensionsForPointer(const std::string &pointer) const {
  auto local_found = pointer_dimensions_.find(pointer);
  if (local_found != pointer_dimensions_.end()) {
    return local_found->second;
  }
  if (startsWith(pointer, "@")) {
    auto found = global_dimensions_.find(stripSigil(pointer));
    if (found == global_dimensions_.end()) {
      throw RiscvError("unknown global pointer: " + pointer);
    }
    return found->second;
  }
  throw RiscvError("unknown pointer value: " + pointer);
}

void StackFrame::assignStackSlots() {
  std::set<std::string> stack_values;
  for (const std::string &param : function_.params) {
    stack_sizes_[param] = 4;
    if (!hasRegisterValue(param) || registers_.needsCallSaveSlot(param)) {
      stack_values.insert(param);
    }
  }
  for (size_t i = 0; i < function_.params.size(); ++i) {
    if (i < function_.param_types.size() &&
        startsWith(function_.param_types[i], "*")) {
      pointer_dimensions_[function_.params[i]] =
          parsePointerTypeDimensions(function_.param_types[i]);
    }
  }
  for (const std::string &line : function_.instructions) {
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.size() >= 3 && parts[1] == "=") {
      stack_sizes_[parts[0]] = 4;
      if (parts[2] == "alloc") {
        stack_values.insert(parts[0]);
        size_t type_begin = line.find("alloc ");
        std::string type = trim(line.substr(type_begin + 6));
        std::vector<int> dimensions = parseTypeDimensions(type);
        if (!dimensions.empty()) {
          stack_sizes_[parts[0]] = elementCount(dimensions) * 4;
          pointer_dimensions_[parts[0]] = dimensions;
          aggregate_allocs_.insert(parts[0]);
        }
      } else if (parts[2] == "getelemptr") {
        stack_values.insert(parts[0]);
        std::vector<int> base_dims = dimensionsForPointer(parts[3]);
        if (!base_dims.empty()) {
          pointer_dimensions_[parts[0]] =
              std::vector<int>(base_dims.begin() + 1, base_dims.end());
        } else {
          pointer_dimensions_[parts[0]] = {};
        }
      } else if (parts[2] == "getptr") {
        stack_values.insert(parts[0]);
        pointer_dimensions_[parts[0]] = dimensionsForPointer(parts[3]);
      } else if (!hasRegisterValue(parts[0])) {
        stack_values.insert(parts[0]);
      }
      if (registers_.needsCallSaveSlot(parts[0])) {
        stack_values.insert(parts[0]);
      }
    }
    if (line.find("call @") != std::string::npos) {
      has_call_ = true;
      CallInstruction call = parseCallInstruction(line);
      if (call.args.size() > max_call_args_) {
        max_call_args_ = call.args.size();
      }
    }
  }

  outgoing_arg_size_ =
      max_call_args_ > 8 ? static_cast<int>((max_call_args_ - 8) * 4) : 0;
  int next_offset = outgoing_arg_size_;
  for (const std::string &value : stack_values) {
    stack_offsets_[value] = next_offset;
    next_offset += localStackSizeOf(value);
  }
  if (has_call_) {
    ra_offset_ = next_offset;
    next_offset += 4;
  }
  frame_size_ = ((next_offset + 15) / 16) * 16;
}

int StackFrame::localStackSizeOf(const std::string &value) const {
  auto found = stack_sizes_.find(value);
  if (found == stack_sizes_.end()) {
    return 4;
  }
  return found->second;
}

} // namespace compiler::riscv
