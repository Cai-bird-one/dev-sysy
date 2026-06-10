#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace compiler::riscv {

std::string trim(const std::string &text);
bool startsWith(const std::string &text, const std::string &prefix);
bool isIntegerLiteral(const std::string &text);
std::string stripSigil(const std::string &name);
std::vector<std::string> splitWhitespace(const std::string &line);
std::vector<std::string> splitCommaList(const std::string &text);

std::string koopaBinaryToRiscv(const std::string &op);
bool isComparison(const std::string &op);

size_t findTopLevelComma(const std::string &text, size_t begin = 0);
std::vector<int> parseTypeDimensions(const std::string &type);
std::vector<int> parsePointerTypeDimensions(const std::string &type);
int elementCount(const std::vector<int> &dimensions, size_t begin = 0);
std::vector<int> parseInitializerValues(const std::string &initializer,
                                        int expected_count);

} // namespace compiler::riscv
