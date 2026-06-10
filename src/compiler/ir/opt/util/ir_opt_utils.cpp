#include "compiler/ir/opt/util/ir_opt_utils.h"

#include "compiler/ir/koopa_generator.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace compiler::ir::opt {

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

bool isInteger(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  size_t index = text[0] == '-' ? 1 : 0;
  if (index == text.size()) {
    return false;
  }
  for (; index < text.size(); ++index) {
    if (!std::isdigit(static_cast<unsigned char>(text[index]))) {
      return false;
    }
  }
  return true;
}

bool isValueName(const std::string &text) {
  return startsWith(text, "%") || startsWith(text, "@");
}

bool isLabel(const std::string &line) {
  return !line.empty() && line.back() == ':';
}

bool isTerminator(const std::string &line) {
  return startsWith(line, "ret") || startsWith(line, "jump ") ||
         startsWith(line, "br ");
}

std::vector<std::string> splitWhitespace(const std::string &text) {
  std::istringstream input(text);
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

Assignment parseAssignment(const std::string &line) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() < 3 || parts[1] != "=") {
    return {};
  }
  Assignment assignment;
  assignment.valid = true;
  assignment.result = parts[0];
  assignment.op = parts[2];
  assignment.args.assign(parts.begin() + 3, parts.end());
  return assignment;
}

std::string formatAssignment(const Assignment &assignment) {
  std::ostringstream output;
  output << assignment.result << " = " << assignment.op;
  for (size_t i = 0; i < assignment.args.size(); ++i) {
    output << (i == 0 ? " " : ", ") << assignment.args[i];
  }
  return output.str();
}

bool isSideEffectFree(const Assignment &assignment) {
  if (!assignment.valid) {
    return false;
  }
  return assignment.op == "add" || assignment.op == "sub" ||
         assignment.op == "mul" || assignment.op == "div" ||
         assignment.op == "mod" || assignment.op == "eq" ||
         assignment.op == "ne" || assignment.op == "lt" ||
         assignment.op == "gt" || assignment.op == "le" ||
         assignment.op == "ge" || assignment.op == "load" ||
         assignment.op == "getelemptr" || assignment.op == "getptr";
}

std::string replaceOperands(
    const std::string &line,
    const std::vector<std::pair<std::string, std::string>> &replacements) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.empty()) {
    return line;
  }
  for (std::string &part : parts) {
    for (const auto &replacement : replacements) {
      if (part == replacement.first) {
        part = replacement.second;
      }
    }
  }
  if (parts.size() >= 3 && parts[1] == "=") {
    Assignment assignment;
    assignment.valid = true;
    assignment.result = parts[0];
    assignment.op = parts[2];
    assignment.args.assign(parts.begin() + 3, parts.end());
    return formatAssignment(assignment);
  }
  std::ostringstream output;
  for (size_t i = 0; i < parts.size(); ++i) {
    output << (i == 0 ? "" : " ") << parts[i];
    if ((parts[0] == "br" && i >= 1 && i < 3) ||
        (parts[0] == "store" && i == 1)) {
      output << ",";
    }
  }
  return output.str();
}

long long evaluateBinary(const std::string &op, long long lhs, long long rhs) {
  if (op == "add") return lhs + rhs;
  if (op == "sub") return lhs - rhs;
  if (op == "mul") return lhs * rhs;
  if (op == "div") return rhs == 0 ? 0 : lhs / rhs;
  if (op == "mod") return rhs == 0 ? 0 : lhs % rhs;
  if (op == "eq") return lhs == rhs;
  if (op == "ne") return lhs != rhs;
  if (op == "lt") return lhs < rhs;
  if (op == "gt") return lhs > rhs;
  if (op == "le") return lhs <= rhs;
  if (op == "ge") return lhs >= rhs;
  throw IrError("unsupported constant binary op: " + op);
}

} // namespace compiler::ir::opt
