#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace compiler::ir {

struct KoopaTensorShape {
  std::vector<int> dims;

  int elementCount() const;
  std::string str() const;
  bool operator==(const KoopaTensorShape &other) const;
  bool operator!=(const KoopaTensorShape &other) const;
};

enum class KoopaTensorOpKind {
  Add,
  AddRelu,
  Broadcast,
  Matmul,
  MatmulTa,
  MatmulTb,
  MatmulTaTb,
  MatmulBias,
  MatmulBiasTa,
  MatmulBiasTb,
  MatmulBiasTaTb,
  MatmulBiasRelu,
  MatmulBiasReluTa,
  MatmulBiasReluTb,
  MatmulBiasReluTaTb,
  MatmulRelu,
  MatmulReluTa,
  MatmulReluTb,
  MatmulReluTaTb,
  Relu,
  Reshape,
  Transpose,
};

struct KoopaTensorOp {
  KoopaTensorOpKind kind;
  std::string result;
  std::vector<std::string> operands;
  KoopaTensorShape shape;
};

struct KoopaTensorInfo {
  std::map<std::string, KoopaTensorShape> shapes;
  std::vector<KoopaTensorOp> operations;
};

class KoopaTensorError : public std::runtime_error {
public:
  explicit KoopaTensorError(const std::string &message);
};

bool isTensorDialectLine(const std::string &line);
void parseTensorDialectLine(KoopaTensorInfo &info, const std::string &line);
std::string formatTensorInfo(const KoopaTensorInfo &info);
std::string formatTensorShapeDecl(const std::string &value,
                                  const KoopaTensorShape &shape);
std::string formatTensorOpDecl(const KoopaTensorOp &operation);
bool isTensorMatmulKind(KoopaTensorOpKind kind);
bool tensorMatmulHasBias(KoopaTensorOpKind kind);
bool tensorMatmulHasRelu(KoopaTensorOpKind kind);
bool tensorMatmulTransposesLhs(KoopaTensorOpKind kind);
bool tensorMatmulTransposesRhs(KoopaTensorOpKind kind);
KoopaTensorOpKind tensorMatmulKind(bool has_bias, bool has_relu,
                                   bool transpose_lhs, bool transpose_rhs);
std::string tensorOpKindName(KoopaTensorOpKind kind);

} // namespace compiler::ir
