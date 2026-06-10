#include "compiler/riscv/util/riscv_utils.h"

#include "compiler/riscv/riscv_generator.h"

#include <cctype>
#include <sstream>

namespace compiler::riscv {

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
    int depth = 0;
    size_t comma = std::string::npos;
    for (size_t i = begin; i < text.size(); ++i) {
      if (text[i] == '[' || text[i] == '(' || text[i] == '{') {
        ++depth;
      } else if (text[i] == ']' || text[i] == ')' || text[i] == '}') {
        --depth;
      } else if (text[i] == ',' && depth == 0) {
        comma = i;
        break;
      }
    }
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

size_t findTopLevelComma(const std::string &text, size_t begin) {
  int depth = 0;
  for (size_t i = begin; i < text.size(); ++i) {
    if (text[i] == '[' || text[i] == '{' || text[i] == '(') {
      ++depth;
    } else if (text[i] == ']' || text[i] == '}' || text[i] == ')') {
      --depth;
    } else if (text[i] == ',' && depth == 0) {
      return i;
    }
  }
  return std::string::npos;
}

std::vector<int> parseTypeDimensions(const std::string &type) {
  std::string text = trim(type);
  if (text == "i32") {
    return {};
  }
  if (text.empty() || text.front() != '[' || text.back() != ']') {
    throw RiscvError("unsupported Koopa type: " + type);
  }
  std::string inside = text.substr(1, text.size() - 2);
  size_t comma = findTopLevelComma(inside);
  if (comma == std::string::npos) {
    throw RiscvError("invalid array type: " + type);
  }
  std::string element_type = trim(inside.substr(0, comma));
  std::string size_text = trim(inside.substr(comma + 1));
  if (!isIntegerLiteral(size_text)) {
    throw RiscvError("invalid array size: " + type);
  }
  std::vector<int> dimensions;
  dimensions.push_back(std::stoi(size_text));
  std::vector<int> tail = parseTypeDimensions(element_type);
  dimensions.insert(dimensions.end(), tail.begin(), tail.end());
  return dimensions;
}

std::vector<int> parsePointerTypeDimensions(const std::string &type) {
  if (!startsWith(type, "*")) {
    throw RiscvError("expected pointer type: " + type);
  }
  return parseTypeDimensions(type.substr(1));
}

int elementCount(const std::vector<int> &dimensions, size_t begin) {
  int count = 1;
  for (size_t i = begin; i < dimensions.size(); ++i) {
    count *= dimensions[i];
  }
  return count;
}

std::vector<int> parseInitializerValues(const std::string &initializer,
                                        int expected_count) {
  if (trim(initializer) == "zeroinit") {
    return std::vector<int>(expected_count, 0);
  }
  std::vector<int> values;
  for (size_t i = 0; i < initializer.size();) {
    if (initializer[i] == '-' ||
        std::isdigit(static_cast<unsigned char>(initializer[i]))) {
      size_t begin = i;
      if (initializer[i] == '-') {
        ++i;
      }
      while (i < initializer.size() &&
             std::isdigit(static_cast<unsigned char>(initializer[i]))) {
        ++i;
      }
      values.push_back(std::stoi(initializer.substr(begin, i - begin)));
    } else {
      ++i;
    }
  }
  if (static_cast<int>(values.size()) != expected_count) {
    throw RiscvError("global initializer size does not match array type");
  }
  return values;
}

} // namespace compiler::riscv
