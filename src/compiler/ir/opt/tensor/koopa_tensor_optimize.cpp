#include "compiler/ir/opt/tensor/koopa_tensor_optimize.h"

#include "compiler/ir/tensor/koopa_tensor_verify.h"

#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace compiler::ir::opt {
namespace {

std::set<std::string> operationResults(const KoopaTensorInfo &info) {
  std::set<std::string> results;
  for (const KoopaTensorOp &operation : info.operations) {
    results.insert(operation.result);
  }
  return results;
}

std::string resolveAlias(const std::map<std::string, std::string> &aliases,
                         const std::string &value) {
  std::string current = value;
  std::set<std::string> seen;
  while (seen.insert(current).second) {
    auto found = aliases.find(current);
    if (found == aliases.end()) {
      return current;
    }
    current = found->second;
  }
  return current;
}

KoopaTensorShape knownShape(const KoopaTensorInfo &info,
                            const std::string &value) {
  auto found = info.shapes.find(value);
  if (found == info.shapes.end()) {
    throw KoopaTensorError("unknown tensor value: " + value);
  }
  return found->second;
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

bool isCommutative(KoopaTensorOpKind kind) {
  return kind == KoopaTensorOpKind::Add ||
         kind == KoopaTensorOpKind::AddRelu;
}

bool isReluProducing(KoopaTensorOpKind kind) {
  return kind == KoopaTensorOpKind::AddRelu ||
         (isTensorMatmulKind(kind) && tensorMatmulHasRelu(kind)) ||
         kind == KoopaTensorOpKind::Relu;
}

void canonicalizeOperands(KoopaTensorOp &operation) {
  if (isCommutative(operation.kind) && operation.operands.size() == 2 &&
      operation.operands[1] < operation.operands[0]) {
    std::swap(operation.operands[0], operation.operands[1]);
  }
}

std::string operationKey(KoopaTensorOp operation) {
  canonicalizeOperands(operation);
  std::ostringstream key;
  key << tensorOpKindName(operation.kind) << ':' << operation.shape.str();
  for (const std::string &operand : operation.operands) {
    key << ':' << operand;
  }
  return key.str();
}

void appendOperation(KoopaTensorInfo &info, const KoopaTensorOp &operation,
                     std::map<std::string, KoopaTensorOp> &definitions) {
  verifyTensorOperation(info, operation);
  info.shapes[operation.result] = operation.shape;
  info.operations.push_back(operation);
  definitions[operation.result] = operation;
}

KoopaTensorInfo pruneDeadOperations(const KoopaTensorInfo &info,
                                    const std::set<std::string> &hidden) {
  std::map<std::string, KoopaTensorOp> definitions;
  std::set<std::string> op_results = operationResults(info);
  std::set<std::string> used_by_operations;
  for (const KoopaTensorOp &operation : info.operations) {
    definitions[operation.result] = operation;
    for (const std::string &operand : operation.operands) {
      if (op_results.find(operand) != op_results.end()) {
        used_by_operations.insert(operand);
      }
    }
  }

  std::set<std::string> needed_results;
  auto mark = [&](const auto &self, const std::string &value) -> void {
    auto found = definitions.find(value);
    if (found == definitions.end() || !needed_results.insert(value).second) {
      return;
    }
    for (const std::string &operand : found->second.operands) {
      self(self, operand);
    }
  };

  for (const KoopaTensorOp &operation : info.operations) {
    bool externally_visible =
        used_by_operations.find(operation.result) == used_by_operations.end() &&
        hidden.find(operation.result) == hidden.end();
    if (externally_visible) {
      mark(mark, operation.result);
    }
  }

  KoopaTensorInfo pruned;
  for (const auto &[value, shape] : info.shapes) {
    if (op_results.find(value) == op_results.end()) {
      pruned.shapes[value] = shape;
    }
  }
  for (const KoopaTensorOp &operation : info.operations) {
    if (needed_results.find(operation.result) == needed_results.end()) {
      continue;
    }
    pruned.shapes[operation.result] = operation.shape;
    pruned.operations.push_back(operation);
  }
  return pruned;
}

bool maybeFoldUnaryAlias(const KoopaTensorInfo &out,
                         const std::map<std::string, KoopaTensorOp> &defs,
                         KoopaTensorOp &operation,
                         std::map<std::string, std::string> &aliases,
                         std::set<std::string> &hidden) {
  if (operation.kind == KoopaTensorOpKind::Reshape ||
      operation.kind == KoopaTensorOpKind::Broadcast) {
    if (knownShape(out, operation.operands[0]) == operation.shape) {
      aliases[operation.result] = operation.operands[0];
      return true;
    }
  }

  auto input = defs.find(operation.operands[0]);
  if (input == defs.end()) {
    return false;
  }

  if (operation.kind == KoopaTensorOpKind::Reshape &&
      input->second.kind == KoopaTensorOpKind::Reshape) {
    operation.operands[0] = input->second.operands[0];
    hidden.insert(input->second.result);
    return false;
  }

  if (operation.kind == KoopaTensorOpKind::Broadcast &&
      input->second.kind == KoopaTensorOpKind::Broadcast &&
      canBroadcastTo(knownShape(out, input->second.operands[0]),
                     operation.shape)) {
    operation.operands[0] = input->second.operands[0];
    hidden.insert(input->second.result);
    return false;
  }

  if (operation.kind == KoopaTensorOpKind::Transpose &&
      input->second.kind == KoopaTensorOpKind::Transpose &&
      knownShape(out, input->second.operands[0]) == operation.shape) {
    aliases[operation.result] = input->second.operands[0];
    hidden.insert(input->second.result);
    return true;
  }

  if (operation.kind == KoopaTensorOpKind::Relu &&
      input->second.kind == KoopaTensorOpKind::Relu) {
    operation.operands[0] = input->second.operands[0];
    hidden.insert(input->second.result);
    return false;
  }

  if (operation.kind == KoopaTensorOpKind::Relu &&
      isReluProducing(input->second.kind)) {
    std::string result = operation.result;
    operation = input->second;
    operation.result = result;
    hidden.insert(input->second.result);
    return false;
  }

  return false;
}

std::string stripBroadcastOperand(
    const std::map<std::string, KoopaTensorOp> &defs,
    const std::string &value, std::set<std::string> &hidden) {
  auto broadcast = defs.find(value);
  if (broadcast != defs.end() &&
      broadcast->second.kind == KoopaTensorOpKind::Broadcast) {
    hidden.insert(broadcast->second.result);
    return broadcast->second.operands[0];
  }
  return value;
}

void absorbMatmulTransposeOperands(
    const std::map<std::string, KoopaTensorOp> &defs, KoopaTensorOp &operation,
    std::set<std::string> &hidden) {
  if (!isTensorMatmulKind(operation.kind)) {
    return;
  }

  bool transpose_lhs = tensorMatmulTransposesLhs(operation.kind);
  bool transpose_rhs = tensorMatmulTransposesRhs(operation.kind);

  for (size_t operand_index = 0; operand_index < 2; ++operand_index) {
    auto operand = defs.find(operation.operands[operand_index]);
    if (operand == defs.end() ||
        operand->second.kind != KoopaTensorOpKind::Transpose) {
      continue;
    }

    operation.operands[operand_index] = operand->second.operands[0];
    if (operand_index == 0) {
      transpose_lhs = !transpose_lhs;
    } else {
      transpose_rhs = !transpose_rhs;
    }
    hidden.insert(operand->second.result);
  }

  operation.kind =
      tensorMatmulKind(tensorMatmulHasBias(operation.kind),
                       tensorMatmulHasRelu(operation.kind), transpose_lhs,
                       transpose_rhs);
}

bool fuseAddWithMatmul(const std::map<std::string, KoopaTensorOp> &defs,
                       KoopaTensorOp &operation,
                       bool fused_has_relu,
                       std::set<std::string> &hidden) {
  if (operation.kind != KoopaTensorOpKind::Add &&
      operation.kind != KoopaTensorOpKind::Relu) {
    return false;
  }

  auto add = operation.kind == KoopaTensorOpKind::Add
                 ? defs.end()
                 : defs.find(operation.operands[0]);
  const KoopaTensorOp *add_operation = nullptr;
  if (operation.kind == KoopaTensorOpKind::Add) {
    add_operation = &operation;
  } else if (add != defs.end() && add->second.kind == KoopaTensorOpKind::Add) {
    add_operation = &add->second;
  }
  if (add_operation == nullptr) {
    return false;
  }

  for (size_t matmul_index = 0; matmul_index < 2; ++matmul_index) {
    const std::string &matmul_value = add_operation->operands[matmul_index];
    auto matmul = defs.find(matmul_value);
    if (matmul == defs.end() || !isTensorMatmulKind(matmul->second.kind) ||
        tensorMatmulHasBias(matmul->second.kind) ||
        tensorMatmulHasRelu(matmul->second.kind)) {
      continue;
    }

    const std::string &bias = add_operation->operands[1 - matmul_index];
    operation.kind =
        tensorMatmulKind(true, fused_has_relu,
                         tensorMatmulTransposesLhs(matmul->second.kind),
                         tensorMatmulTransposesRhs(matmul->second.kind));
    operation.operands = {matmul->second.operands[0], matmul->second.operands[1],
                          stripBroadcastOperand(defs, bias, hidden)};
    if (add_operation != &operation) {
      hidden.insert(add_operation->result);
    }
    hidden.insert(matmul->second.result);
    return true;
  }
  return false;
}

void maybeFuseTensorOperation(
    const std::map<std::string, KoopaTensorOp> &defs, KoopaTensorOp &operation,
    std::set<std::string> &hidden) {
  if (operation.kind == KoopaTensorOpKind::Add &&
      fuseAddWithMatmul(defs, operation, false, hidden)) {
    return;
  }

  if (operation.kind != KoopaTensorOpKind::Relu) {
    return;
  }

  auto input = defs.find(operation.operands[0]);
  if (input == defs.end()) {
    return;
  }

  if (isTensorMatmulKind(input->second.kind) &&
      tensorMatmulHasBias(input->second.kind) &&
      !tensorMatmulHasRelu(input->second.kind)) {
    operation.kind =
        tensorMatmulKind(true, true,
                         tensorMatmulTransposesLhs(input->second.kind),
                         tensorMatmulTransposesRhs(input->second.kind));
    operation.operands = input->second.operands;
    hidden.insert(input->second.result);
    return;
  }

  if (isTensorMatmulKind(input->second.kind) &&
      !tensorMatmulHasBias(input->second.kind) &&
      !tensorMatmulHasRelu(input->second.kind)) {
    operation.kind =
        tensorMatmulKind(false, true,
                         tensorMatmulTransposesLhs(input->second.kind),
                         tensorMatmulTransposesRhs(input->second.kind));
    operation.operands = input->second.operands;
    hidden.insert(input->second.result);
    return;
  }

  if (fuseAddWithMatmul(defs, operation, true, hidden)) {
    return;
  }

  if (input->second.kind == KoopaTensorOpKind::Add) {
    operation.kind = KoopaTensorOpKind::AddRelu;
    operation.operands = input->second.operands;
    hidden.insert(input->second.result);
  }
}

} // namespace

KoopaTensorInfo optimizeTensorInfo(const KoopaTensorInfo &info) {
  KoopaTensorInfo out;
  std::set<std::string> original_results = operationResults(info);
  for (const auto &[value, shape] : info.shapes) {
    if (original_results.find(value) == original_results.end()) {
      out.shapes[value] = shape;
    }
  }

  std::map<std::string, std::string> aliases;
  std::map<std::string, KoopaTensorOp> definitions;
  std::map<std::string, std::string> equivalent_operations;
  std::set<std::string> hidden_results;

  for (KoopaTensorOp operation : info.operations) {
    for (std::string &operand : operation.operands) {
      operand = resolveAlias(aliases, operand);
    }

    if (maybeFoldUnaryAlias(out, definitions, operation, aliases,
                            hidden_results)) {
      continue;
    }
    maybeFuseTensorOperation(definitions, operation, hidden_results);
    absorbMatmulTransposeOperands(definitions, operation, hidden_results);
    canonicalizeOperands(operation);

    std::string key = operationKey(operation);
    auto equivalent = equivalent_operations.find(key);
    if (equivalent != equivalent_operations.end()) {
      aliases[operation.result] = equivalent->second;
      hidden_results.insert(operation.result);
      continue;
    }
    appendOperation(out, operation, definitions);
    equivalent_operations[key] = operation.result;
  }

  return pruneDeadOperations(out, hidden_results);
}

} // namespace compiler::ir::opt
