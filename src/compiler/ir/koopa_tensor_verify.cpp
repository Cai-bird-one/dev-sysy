#include "compiler/ir/koopa_tensor_extension_internal.h"

namespace compiler::ir {
namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw KoopaTensorError(message);
  }
}

bool canBroadcastTo(const KoopaTensorShape &from,
                    const KoopaTensorShape &to) {
  if (from.dims.size() > to.dims.size()) {
    return false;
  }
  size_t offset = to.dims.size() - from.dims.size();
  for (size_t i = 0; i < from.dims.size(); ++i) {
    int source = from.dims[i];
    int target = to.dims[i + offset];
    if (source != 1 && source != target) {
      return false;
    }
  }
  return true;
}

KoopaTensorShape knownShape(const KoopaTensorInfo &info,
                            const std::string &value) {
  auto found = info.shapes.find(value);
  require(found != info.shapes.end(), "unknown tensor value: " + value);
  return found->second;
}

} // namespace

void verifyTensorOperation(const KoopaTensorInfo &info,
                           const KoopaTensorOp &operation) {
  auto arity = [&](size_t expected) {
    require(operation.operands.size() == expected,
            tensorOpKindName(operation.kind) + " operand count mismatch");
  };

  if (operation.kind == KoopaTensorOpKind::Matmul ||
      operation.kind == KoopaTensorOpKind::MatmulBiasRelu) {
    arity(operation.kind == KoopaTensorOpKind::Matmul ? 2 : 3);
    if (operation.kind == KoopaTensorOpKind::MatmulBiasRelu) {
      require(canBroadcastTo(knownShape(info, operation.operands[2]),
                             operation.shape),
              "tensor.matmul_bias_relu bias shape is not compatible");
    }
    KoopaTensorShape lhs = knownShape(info, operation.operands[0]);
    KoopaTensorShape rhs = knownShape(info, operation.operands[1]);
    require(lhs.dims.size() == 2 && rhs.dims.size() == 2,
            tensorOpKindName(operation.kind) + " expects rank-2 matrix operands");
    require(lhs.dims[1] == rhs.dims[0],
            tensorOpKindName(operation.kind) + " inner dimensions do not match");
    require(operation.shape == KoopaTensorShape{{lhs.dims[0], rhs.dims[1]}},
            tensorOpKindName(operation.kind) + " result shape mismatch");
  } else if (operation.kind == KoopaTensorOpKind::Add) {
    arity(2);
    KoopaTensorShape lhs = knownShape(info, operation.operands[0]);
    KoopaTensorShape rhs = knownShape(info, operation.operands[1]);
    require(lhs == rhs && operation.shape == lhs,
            "tensor.add requires identical shapes");
  } else if (operation.kind == KoopaTensorOpKind::Reshape) {
    arity(1);
    KoopaTensorShape input = knownShape(info, operation.operands[0]);
    require(input.elementCount() == operation.shape.elementCount(),
            "tensor.reshape cannot change element count");
  } else if (operation.kind == KoopaTensorOpKind::Transpose) {
    arity(1);
    KoopaTensorShape input = knownShape(info, operation.operands[0]);
    require(input.dims.size() == 2, "tensor.transpose expects rank-2 input");
    require(operation.shape == KoopaTensorShape{{input.dims[1], input.dims[0]}},
            "tensor.transpose result shape mismatch");
  } else if (operation.kind == KoopaTensorOpKind::Broadcast) {
    arity(1);
    require(canBroadcastTo(knownShape(info, operation.operands[0]),
                           operation.shape),
            "tensor.broadcast shape is not compatible");
  } else if (operation.kind == KoopaTensorOpKind::Relu) {
    arity(1);
    require(operation.shape == knownShape(info, operation.operands[0]),
            "tensor.relu preserves input shape");
  }
}

} // namespace compiler::ir
