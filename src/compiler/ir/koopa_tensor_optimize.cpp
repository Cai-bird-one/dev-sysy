#include "compiler/ir/koopa_tensor_extension.h"

#include "compiler/ir/koopa_tensor_extension_internal.h"

#include <map>
#include <set>
#include <utility>

namespace compiler::ir {
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

  if (operation.kind == KoopaTensorOpKind::Transpose &&
      input->second.kind == KoopaTensorOpKind::Transpose &&
      knownShape(out, input->second.operands[0]) == operation.shape) {
    aliases[operation.result] = input->second.operands[0];
    hidden.insert(input->second.result);
    return true;
  }

  if (operation.kind == KoopaTensorOpKind::Relu &&
      input->second.kind == KoopaTensorOpKind::Relu) {
    aliases[operation.result] = operation.operands[0];
    hidden.insert(input->second.result);
    return true;
  }

  return false;
}

void maybeFuseMatmulBiasRelu(
    const std::map<std::string, KoopaTensorOp> &defs, KoopaTensorOp &operation,
    std::set<std::string> &hidden) {
  if (operation.kind != KoopaTensorOpKind::Relu) {
    return;
  }

  auto add = defs.find(operation.operands[0]);
  if (add == defs.end() || add->second.kind != KoopaTensorOpKind::Add) {
    return;
  }

  for (size_t matmul_index = 0; matmul_index < 2; ++matmul_index) {
    const std::string &matmul_value = add->second.operands[matmul_index];
    auto matmul = defs.find(matmul_value);
    if (matmul == defs.end() ||
        matmul->second.kind != KoopaTensorOpKind::Matmul) {
      continue;
    }

    const std::string &bias = add->second.operands[1 - matmul_index];
    operation.kind = KoopaTensorOpKind::MatmulBiasRelu;
    operation.operands = {matmul->second.operands[0], matmul->second.operands[1],
                          bias};
    hidden.insert(add->second.result);
    hidden.insert(matmul->second.result);
    return;
  }
}

} // namespace

KoopaTensorInfo
KoopaTensorExtension::optimize(const KoopaTensorInfo &info) const {
  KoopaTensorInfo out;
  std::set<std::string> original_results = operationResults(info);
  for (const auto &[value, shape] : info.shapes) {
    if (original_results.find(value) == original_results.end()) {
      out.shapes[value] = shape;
    }
  }

  std::map<std::string, std::string> aliases;
  std::map<std::string, KoopaTensorOp> definitions;
  std::set<std::string> hidden_results;

  for (KoopaTensorOp operation : info.operations) {
    for (std::string &operand : operation.operands) {
      operand = resolveAlias(aliases, operand);
    }

    if (maybeFoldUnaryAlias(out, definitions, operation, aliases,
                            hidden_results)) {
      continue;
    }
    maybeFuseMatmulBiasRelu(definitions, operation, hidden_results);
    appendOperation(out, operation, definitions);
  }

  return pruneDeadOperations(out, hidden_results);
}

} // namespace compiler::ir
