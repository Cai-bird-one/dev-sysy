#include "compiler/ir/util/ir_utils.h"

#include "compiler/ir/koopa_generator.h"

#include <cctype>

namespace compiler::ir {

std::string toOperand(long long value) { return std::to_string(value); }

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

size_t findTopLevelComma(const std::string &text, size_t begin) {
  int depth = 0;
  for (size_t i = begin; i < text.size(); ++i) {
    if (text[i] == '[') {
      ++depth;
    } else if (text[i] == ']') {
      --depth;
    } else if (text[i] == ',' && depth == 0) {
      return i;
    }
  }
  return std::string::npos;
}

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

std::string koopaOp(const std::string &token) {
  if (token == "PLUS") {
    return "add";
  }
  if (token == "MINUS") {
    return "sub";
  }
  if (token == "STAR") {
    return "mul";
  }
  if (token == "SLASH") {
    return "div";
  }
  if (token == "PERCENT") {
    return "mod";
  }
  if (token == "LT") {
    return "lt";
  }
  if (token == "GT") {
    return "gt";
  }
  if (token == "LE") {
    return "le";
  }
  if (token == "GE") {
    return "ge";
  }
  if (token == "EQ") {
    return "eq";
  }
  if (token == "NE") {
    return "ne";
  }
  if (token == "AND") {
    return "and";
  }
  if (token == "OR") {
    return "or";
  }
  throw IrError("unsupported binary operator: " + token);
}

long long foldBinary(const std::string &op, long long lhs, long long rhs) {
  if (op == "PLUS") {
    return lhs + rhs;
  }
  if (op == "MINUS") {
    return lhs - rhs;
  }
  if (op == "STAR") {
    return lhs * rhs;
  }
  if (op == "SLASH") {
    return lhs / rhs;
  }
  if (op == "PERCENT") {
    return lhs % rhs;
  }
  if (op == "LT") {
    return lhs < rhs;
  }
  if (op == "GT") {
    return lhs > rhs;
  }
  if (op == "LE") {
    return lhs <= rhs;
  }
  if (op == "GE") {
    return lhs >= rhs;
  }
  if (op == "EQ") {
    return lhs == rhs;
  }
  if (op == "NE") {
    return lhs != rhs;
  }
  if (op == "AND") {
    return (lhs != 0) && (rhs != 0);
  }
  if (op == "OR") {
    return (lhs != 0) || (rhs != 0);
  }
  throw IrError("unsupported binary operator: " + op);
}

long long expectConstant(const Value &value, const std::string &context) {
  if (!value.constant) {
    throw IrError(context + " must be a constant expression");
  }
  return value.const_value;
}

long long elementCount(const std::vector<long long> &dimensions, size_t begin) {
  long long count = 1;
  for (size_t i = begin; i < dimensions.size(); ++i) {
    count *= dimensions[i];
  }
  return count;
}

std::string arrayType(const std::vector<long long> &dimensions, size_t begin) {
  if (begin == dimensions.size()) {
    return "i32";
  }
  return "[" + arrayType(dimensions, begin + 1) + ", " +
         std::to_string(dimensions[begin]) + "]";
}

std::vector<long long> parseArrayTypeDimensions(const std::string &type) {
  std::string text = trim(type);
  if (text == "i32") {
    return {};
  }
  if (text.empty() || text.front() != '[' || text.back() != ']') {
    throw IrError("invalid array type: " + type);
  }
  std::string inner = text.substr(1, text.size() - 2);
  size_t comma = findTopLevelComma(inner);
  if (comma == std::string::npos) {
    throw IrError("invalid array type: " + type);
  }
  std::vector<long long> dimensions;
  dimensions.push_back(std::stoll(trim(inner.substr(comma + 1))));
  std::vector<long long> tail =
      parseArrayTypeDimensions(inner.substr(0, comma));
  dimensions.insert(dimensions.end(), tail.begin(), tail.end());
  return dimensions;
}

std::vector<long long> parsePointerTypeDimensions(const std::string &type) {
  if (!startsWith(type, "*")) {
    throw IrError("expected pointer type: " + type);
  }
  return parseArrayTypeDimensions(type.substr(1));
}

} // namespace compiler::ir
