#pragma once

#include <string>
#include <vector>

namespace compiler::ir::opt {

struct Assignment {
  bool valid = false;
  std::string result;
  std::string op;
  std::vector<std::string> args;
};

std::string trim(const std::string &text);
bool startsWith(const std::string &text, const std::string &prefix);
bool isInteger(const std::string &text);
bool isValueName(const std::string &text);
bool isLabel(const std::string &line);
bool isTerminator(const std::string &line);
bool isSideEffectFree(const Assignment &assignment);
std::vector<std::string> splitWhitespace(const std::string &text);
Assignment parseAssignment(const std::string &line);
std::string formatAssignment(const Assignment &assignment);
std::string replaceOperands(
    const std::string &line,
    const std::vector<std::pair<std::string, std::string>> &replacements);
long long evaluateBinary(const std::string &op, long long lhs, long long rhs);

} // namespace compiler::ir::opt
