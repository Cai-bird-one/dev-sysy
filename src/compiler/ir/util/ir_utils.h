#pragma once

#include "compiler/ir/tensor/koopa_tensor_ir.h"
#include "compiler/ir/model/ir_model.h"

#include <cstddef>
#include <string>
#include <vector>

namespace compiler::ir {

std::string toOperand(long long value);
bool startsWith(const std::string &text, const std::string &prefix);
size_t findTopLevelComma(const std::string &text, size_t begin = 0);
std::string trim(const std::string &text);

std::string koopaOp(const std::string &token);
long long foldBinary(const std::string &op, long long lhs, long long rhs);
long long expectConstant(const Value &value, const std::string &context);

long long elementCount(const std::vector<long long> &dimensions,
                       size_t begin = 0);
std::string arrayType(const std::vector<long long> &dimensions,
                      size_t begin = 0);
KoopaTensorShape tensorShapeFromDimensions(
    const std::vector<long long> &dimensions);
std::vector<long long> parseArrayTypeDimensions(const std::string &type);
std::vector<long long> parsePointerTypeDimensions(const std::string &type);

} // namespace compiler::ir
