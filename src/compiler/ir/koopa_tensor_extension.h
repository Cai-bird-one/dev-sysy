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
  Broadcast,
  MatmulBiasRelu,
  Matmul,
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

class KoopaTensorExtension {
public:
  KoopaTensorInfo parse(const std::string &koopa_ir) const;
  void verify(const std::string &koopa_ir) const;
  KoopaTensorInfo optimize(const KoopaTensorInfo &info) const;
  KoopaTensorInfo optimize(const std::string &koopa_ir) const;
  std::string format(const KoopaTensorInfo &info) const;
  std::string annotateShape(const std::string &value,
                            const KoopaTensorShape &shape) const;
  std::string annotateOp(const KoopaTensorOp &operation) const;
};

std::string tensorOpKindName(KoopaTensorOpKind kind);

} // namespace compiler::ir
