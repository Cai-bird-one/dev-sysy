#include "compiler/riscv/riscv_generator.h"

#include <cctype>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace compiler::riscv {
namespace {

struct GlobalVariable {
  std::string name;
  std::string initial_value;
};

struct Function {
  std::string name;
  std::vector<std::string> params;
  std::vector<std::string> instructions;
};

struct Program {
  std::vector<GlobalVariable> globals;
  std::vector<Function> functions;
};

std::string trim(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool isIntegerLiteral(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  size_t start = 0;
  if (text[0] == '-') {
    if (text.size() == 1) {
      return false;
    }
    start = 1;
  }
  for (size_t i = start; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
  }
  return true;
}

std::string stripSigil(const std::string &name) {
  if (name.empty() || (name[0] != '%' && name[0] != '@')) {
    throw RiscvError("invalid Koopa identifier: " + name);
  }
  return name.substr(1);
}

std::vector<std::string> splitWhitespace(const std::string &line) {
  std::istringstream input(line);
  std::vector<std::string> parts;
  std::string part;
  while (input >> part) {
    if (!part.empty() && part.back() == ',') {
      part.pop_back();
    }
    parts.push_back(part);
  }
  return parts;
}

std::vector<std::string> splitCommaList(const std::string &text) {
  std::vector<std::string> result;
  size_t begin = 0;
  while (begin < text.size()) {
    size_t comma = text.find(',', begin);
    std::string item =
        trim(text.substr(begin, comma == std::string::npos ? std::string::npos
                                                           : comma - begin));
    if (!item.empty()) {
      result.push_back(item);
    }
    if (comma == std::string::npos) {
      break;
    }
    begin = comma + 1;
  }
  return result;
}

std::vector<std::string> parseFunctionParams(const std::string &line) {
  size_t left = line.find('(');
  size_t right = line.find(')', left);
  if (left == std::string::npos || right == std::string::npos || right < left) {
    throw RiscvError("invalid function header: " + line);
  }
  std::string params_text = trim(line.substr(left + 1, right - left - 1));
  if (params_text.empty()) {
    return {};
  }

  std::vector<std::string> params;
  for (const std::string &param : splitCommaList(params_text)) {
    size_t colon = param.find(':');
    if (colon == std::string::npos) {
      throw RiscvError("invalid function parameter: " + param);
    }
    std::string name = trim(param.substr(0, colon));
    std::string type = trim(param.substr(colon + 1));
    if (type != "i32") {
      throw RiscvError("unsupported function parameter type: " + param);
    }
    params.push_back(name);
  }
  return params;
}

struct CallInstruction {
  bool has_result = false;
  std::string result;
  std::string callee;
  std::vector<std::string> args;
};

CallInstruction parseCallInstruction(const std::string &line) {
  CallInstruction call;
  size_t call_pos = line.find("call @");
  if (call_pos == std::string::npos) {
    throw RiscvError("invalid call instruction: " + line);
  }

  size_t eq_pos = line.find(" = ");
  if (eq_pos != std::string::npos && eq_pos < call_pos) {
    call.has_result = true;
    call.result = trim(line.substr(0, eq_pos));
  }

  size_t name_begin = call_pos + 5;
  size_t left = line.find('(', name_begin);
  size_t right = line.rfind(')');
  if (left == std::string::npos || right == std::string::npos || right < left) {
    throw RiscvError("invalid call instruction: " + line);
  }
  call.callee = line.substr(name_begin, left - name_begin);
  call.args = splitCommaList(trim(line.substr(left + 1, right - left - 1)));
  return call;
}

Program parseProgram(const std::string &koopa_ir) {
  Program program;
  std::istringstream input(koopa_ir);
  std::string line;
  bool in_function = false;
  Function current_function;

  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (startsWith(line, "global ")) {
      std::vector<std::string> parts = splitWhitespace(line);
      if (parts.size() != 6 || parts[0] != "global" || parts[2] != "=" ||
          parts[3] != "alloc" || parts[4] != "i32") {
        throw RiscvError("unsupported global Koopa IR: " + line);
      }
      std::string initial_value =
          parts[5] == "zeroinit" ? std::string("0") : parts[5];
      if (!isIntegerLiteral(initial_value)) {
        throw RiscvError("unsupported global initializer: " + parts[5]);
      }
      program.globals.push_back({stripSigil(parts[1]), initial_value});
      continue;
    }

    if (startsWith(line, "fun @")) {
      size_t name_begin = line.find('@') + 1;
      size_t name_end = line.find('(', name_begin);
      if (name_begin == std::string::npos || name_end == std::string::npos ||
          name_end <= name_begin) {
        throw RiscvError("invalid function header: " + line);
      }
      current_function = Function{};
      current_function.name = line.substr(name_begin, name_end - name_begin);
      current_function.params = parseFunctionParams(line);
      in_function = true;
      continue;
    }

    if (line == "}" && in_function) {
      program.functions.push_back(std::move(current_function));
      in_function = false;
      continue;
    }

    if (!in_function) {
      continue;
    }

    current_function.instructions.push_back(line);
  }

  if (program.functions.empty()) {
    throw RiscvError("unsupported Koopa IR: expected function definition");
  }
  return program;
}

std::string koopaBinaryToRiscv(const std::string &op) {
  if (op == "add") {
    return "add";
  }
  if (op == "sub") {
    return "sub";
  }
  if (op == "mul") {
    return "mul";
  }
  if (op == "div") {
    return "div";
  }
  if (op == "mod") {
    return "rem";
  }
  if (op == "and") {
    return "and";
  }
  if (op == "or") {
    return "or";
  }
  throw RiscvError("unsupported binary operation: " + op);
}

bool isComparison(const std::string &op) {
  return op == "lt" || op == "gt" || op == "le" || op == "ge" ||
         op == "eq" || op == "ne";
}

class FunctionEmitter {
public:
  explicit FunctionEmitter(Function function) : function_(std::move(function)) {
    assignStackSlots();
  }

  void emit(std::ostream &output) {
    output << "  .text\n"
           << "  .globl " << function_.name << "\n"
           << function_.name << ":\n";
    adjustStack(output, -frame_size_);
    if (has_call_) {
      storeToStack("ra", ra_offset_, output);
    }
    for (size_t i = 0; i < function_.params.size(); ++i) {
      if (i < 8) {
        storeValue("a" + std::to_string(i), function_.params[i], output);
      } else {
        loadFromStack(frame_size_ + static_cast<int>((i - 8) * 4), "t0", output);
        storeValue("t0", function_.params[i], output);
      }
    }

    for (const std::string &line : function_.instructions) {
      emitInstruction(line, output);
    }
  }

private:
  void assignStackSlots() {
    std::set<std::string> stack_values;
    for (const std::string &param : function_.params) {
      stack_values.insert(param);
    }
    for (const std::string &line : function_.instructions) {
      std::vector<std::string> parts = splitWhitespace(line);
      if (parts.size() >= 3 && parts[1] == "=") {
        stack_values.insert(parts[0]);
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
      next_offset += 4;
    }
    if (has_call_) {
      ra_offset_ = next_offset;
      next_offset += 4;
    }
    frame_size_ = ((next_offset + 15) / 16) * 16;
  }

  void emitInstruction(const std::string &line, std::ostream &output) {
    std::vector<std::string> parts = splitWhitespace(line);
    if (parts.empty()) {
      return;
    }

    if (line.back() == ':') {
      std::string label = line.substr(0, line.size() - 1);
      if (label == "%entry") {
        return;
      }
      output << asmLabel(label) << ":\n";
      return;
    }

    if (parts[0] == "br") {
      if (parts.size() != 4) {
        throw RiscvError("invalid branch instruction: " + line);
      }
      loadOperand(parts[1], "t0", output);
      output << "  bnez t0, " << asmLabel(parts[2]) << "\n"
             << "  j " << asmLabel(parts[3]) << "\n";
      return;
    }

    if (parts[0] == "jump") {
      if (parts.size() != 2) {
        throw RiscvError("invalid jump instruction: " + line);
      }
      output << "  j " << asmLabel(parts[1]) << "\n";
      return;
    }

    if (parts[0] == "store") {
      if (parts.size() != 3) {
        throw RiscvError("invalid store instruction: " + line);
      }
      loadOperand(parts[1], "t0", output);
      storeToPointer("t0", parts[2], output);
      return;
    }

    if (parts[0] == "ret") {
      if (parts.size() > 2) {
        throw RiscvError("invalid return instruction: " + line);
      }
      if (parts.size() == 2) {
        loadOperand(parts[1], "a0", output);
      }
      if (has_call_) {
        loadFromStack(ra_offset_, "ra", output);
      }
      adjustStack(output, frame_size_);
      output << "  ret\n";
      return;
    }

    if (startsWith(line, "call @") ||
        (parts.size() >= 3 && parts[1] == "=" && parts[2] == "call")) {
      emitCall(parseCallInstruction(line), output);
      return;
    }

    if (parts.size() >= 3 && parts[1] == "=") {
      const std::string &result = parts[0];
      const std::string &op = parts[2];
      if (op == "alloc") {
        if (parts.size() != 4 || parts[3] != "i32") {
          throw RiscvError("unsupported alloc instruction: " + line);
        }
        return;
      }
      if (op == "load") {
        if (parts.size() != 4) {
          throw RiscvError("invalid load instruction: " + line);
        }
        loadFromPointer(parts[3], "t0", output);
        storeValue("t0", result, output);
        return;
      }
      if (parts.size() == 5) {
        loadOperand(parts[3], "t0", output);
        loadOperand(parts[4], "t1", output);
        emitBinary(op, output);
        storeValue("t0", result, output);
        return;
      }
    }

    throw RiscvError("unsupported Koopa instruction: " + line);
  }

  void emitBinary(const std::string &op, std::ostream &output) {
    if (isComparison(op)) {
      emitComparison(op, output);
      return;
    }
    output << "  " << koopaBinaryToRiscv(op) << " t0, t0, t1\n";
  }

  void emitCall(const CallInstruction &call, std::ostream &output) {
    for (size_t i = 0; i < call.args.size(); ++i) {
      if (i < 8) {
        loadOperand(call.args[i], "a" + std::to_string(i), output);
      } else {
        loadOperand(call.args[i], "t0", output);
        storeToStack("t0", static_cast<int>((i - 8) * 4), output);
      }
    }
    output << "  call " << stripSigil(call.callee) << "\n";
    if (call.has_result) {
      storeValue("a0", call.result, output);
    }
  }

  void emitComparison(const std::string &op, std::ostream &output) {
    if (op == "lt") {
      output << "  slt t0, t0, t1\n";
      return;
    }
    if (op == "gt") {
      output << "  slt t0, t1, t0\n";
      return;
    }
    if (op == "le") {
      output << "  slt t0, t1, t0\n"
             << "  xori t0, t0, 1\n";
      return;
    }
    if (op == "ge") {
      output << "  slt t0, t0, t1\n"
             << "  xori t0, t0, 1\n";
      return;
    }
    if (op == "eq") {
      output << "  sub t0, t0, t1\n"
             << "  seqz t0, t0\n";
      return;
    }
    if (op == "ne") {
      output << "  sub t0, t0, t1\n"
             << "  snez t0, t0\n";
      return;
    }
  }

  std::string asmLabel(const std::string &koopa_label) const {
    return function_.name + "_" + stripSigil(koopa_label);
  }

  void loadOperand(const std::string &operand, const std::string &reg,
                   std::ostream &output) {
    if (isIntegerLiteral(operand)) {
      output << "  li " << reg << ", " << operand << "\n";
      return;
    }
    auto stack_found = stack_offsets_.find(operand);
    if (stack_found != stack_offsets_.end()) {
      loadFromStack(stack_found->second, reg, output);
      return;
    }
    if (startsWith(operand, "@")) {
      output << "  la " << reg << ", " << stripSigil(operand) << "\n"
             << "  lw " << reg << ", 0(" << reg << ")\n";
      return;
    }
    throw RiscvError("unknown Koopa value: " + operand);
  }

  void loadFromPointer(const std::string &pointer, const std::string &reg,
                       std::ostream &output) {
    if (startsWith(pointer, "@")) {
      output << "  la " << reg << ", " << stripSigil(pointer) << "\n"
             << "  lw " << reg << ", 0(" << reg << ")\n";
      return;
    }
    auto found = stack_offsets_.find(pointer);
    if (found == stack_offsets_.end()) {
      throw RiscvError("unknown Koopa pointer: " + pointer);
    }
    loadFromStack(found->second, reg, output);
  }

  void storeToPointer(const std::string &reg, const std::string &pointer,
                      std::ostream &output) {
    if (startsWith(pointer, "@")) {
      output << "  la t1, " << stripSigil(pointer) << "\n"
             << "  sw " << reg << ", 0(t1)\n";
      return;
    }
    storeValue(reg, pointer, output);
  }

  void storeValue(const std::string &reg, const std::string &value,
                  std::ostream &output) {
    auto found = stack_offsets_.find(value);
    if (found == stack_offsets_.end()) {
      throw RiscvError("unknown Koopa stack value: " + value);
    }
    storeToStack(reg, found->second, output);
  }

  bool fitsSigned12Bit(int value) const {
    return value >= -2048 && value <= 2047;
  }

  void loadFromStack(int offset, const std::string &reg, std::ostream &output) {
    if (fitsSigned12Bit(offset)) {
      output << "  lw " << reg << ", " << offset << "(sp)\n";
      return;
    }
    output << "  li t2, " << offset << "\n"
           << "  add t2, sp, t2\n"
           << "  lw " << reg << ", 0(t2)\n";
  }

  void storeToStack(const std::string &reg, int offset, std::ostream &output) {
    if (fitsSigned12Bit(offset)) {
      output << "  sw " << reg << ", " << offset << "(sp)\n";
      return;
    }
    output << "  li t2, " << offset << "\n"
           << "  add t2, sp, t2\n"
           << "  sw " << reg << ", 0(t2)\n";
  }

  void adjustStack(std::ostream &output, int amount) {
    if (amount == 0) {
      return;
    }
    output << "  li t0, " << amount << "\n"
           << "  add sp, sp, t0\n";
  }

  Function function_;
  std::map<std::string, int> stack_offsets_;
  int frame_size_ = 0;
  int ra_offset_ = 0;
  int outgoing_arg_size_ = 0;
  size_t max_call_args_ = 0;
  bool has_call_ = false;
};

} // namespace

std::string RiscvGenerator::generate(const std::string &koopa_ir) const {
  std::ostringstream output;
  generate(koopa_ir, output);
  return output.str();
}

void RiscvGenerator::generate(const std::string &koopa_ir,
                              std::ostream &output) const {
  Program program = parseProgram(koopa_ir);

  if (!program.globals.empty()) {
    output << "  .data\n";
    for (const GlobalVariable &global : program.globals) {
      output << "  .globl " << global.name << "\n"
             << global.name << ":\n"
             << "  .word " << global.initial_value << "\n";
    }
  }

  for (Function &function : program.functions) {
    FunctionEmitter emitter(std::move(function));
    emitter.emit(output);
  }
}

} // namespace compiler::riscv
