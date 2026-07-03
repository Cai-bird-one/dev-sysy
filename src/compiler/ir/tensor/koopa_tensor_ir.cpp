#include "compiler/ir/tensor/koopa_tensor_ir.h"

#include "compiler/ir/tensor/koopa_tensor_verify.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <set>
#include <sstream>

namespace compiler::ir {
namespace {

const std::string kTensorPrefix = "tensor ";

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw KoopaTensorError(message);
  }
}

KoopaTensorShape parseShape(const std::string &text) {
  std::string shape = opt::trim(text);
  require(shape.size() >= std::string("tensor<>").size() &&
              opt::startsWith(shape, "tensor<") && shape.back() == '>',
          "invalid tensor shape: " + text);
  shape = shape.substr(7, shape.size() - 8);
  KoopaTensorShape result;
  std::stringstream input(shape);
  std::string dim;
  while (std::getline(input, dim, 'x')) {
    require(opt::isInteger(dim), "invalid tensor dimension: " + dim);
    int value = std::stoi(dim);
    require(value > 0, "tensor dimension must be positive: " + dim);
    result.dims.push_back(value);
  }
  require(!result.dims.empty(), "tensor shape cannot be rank-0");
  return result;
}

KoopaTensorOpKind parseKind(const std::string &text) {
  if (text == "add") return KoopaTensorOpKind::Add;
  if (text == "add_relu") return KoopaTensorOpKind::AddRelu;
  if (text == "broadcast") return KoopaTensorOpKind::Broadcast;
  if (text == "matmul") return KoopaTensorOpKind::Matmul;
  if (text == "matmul_ta") return KoopaTensorOpKind::MatmulTa;
  if (text == "matmul_tb") return KoopaTensorOpKind::MatmulTb;
  if (text == "matmul_ta_tb") return KoopaTensorOpKind::MatmulTaTb;
  if (text == "matmul_bias") return KoopaTensorOpKind::MatmulBias;
  if (text == "matmul_bias_ta") return KoopaTensorOpKind::MatmulBiasTa;
  if (text == "matmul_bias_tb") return KoopaTensorOpKind::MatmulBiasTb;
  if (text == "matmul_bias_ta_tb") {
    return KoopaTensorOpKind::MatmulBiasTaTb;
  }
  if (text == "matmul_bias_relu") return KoopaTensorOpKind::MatmulBiasRelu;
  if (text == "matmul_bias_relu_ta") {
    return KoopaTensorOpKind::MatmulBiasReluTa;
  }
  if (text == "matmul_bias_relu_tb") {
    return KoopaTensorOpKind::MatmulBiasReluTb;
  }
  if (text == "matmul_bias_relu_ta_tb") {
    return KoopaTensorOpKind::MatmulBiasReluTaTb;
  }
  if (text == "matmul_relu") return KoopaTensorOpKind::MatmulRelu;
  if (text == "matmul_relu_ta") return KoopaTensorOpKind::MatmulReluTa;
  if (text == "matmul_relu_tb") return KoopaTensorOpKind::MatmulReluTb;
  if (text == "matmul_relu_ta_tb") {
    return KoopaTensorOpKind::MatmulReluTaTb;
  }
  if (text == "relu") return KoopaTensorOpKind::Relu;
  if (text == "reshape") return KoopaTensorOpKind::Reshape;
  if (text == "transpose") return KoopaTensorOpKind::Transpose;
  throw KoopaTensorError("unknown tensor op kind: " + text);
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
        opt::trim(text.substr(begin, comma == std::string::npos
                                         ? std::string::npos
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

std::string joinOperands(const std::vector<std::string> &operands) {
  std::ostringstream output;
  for (size_t i = 0; i < operands.size(); ++i) {
    output << (i == 0 ? "" : ", ") << operands[i];
  }
  return output.str();
}

} // namespace

int KoopaTensorShape::elementCount() const {
  int count = 1;
  for (int dim : dims) {
    count *= dim;
  }
  return count;
}

std::string KoopaTensorShape::str() const {
  std::ostringstream output;
  output << "tensor<";
  for (size_t i = 0; i < dims.size(); ++i) {
    output << (i == 0 ? "" : "x") << dims[i];
  }
  output << ">";
  return output.str();
}

bool KoopaTensorShape::operator==(const KoopaTensorShape &other) const {
  return dims == other.dims;
}

bool KoopaTensorShape::operator!=(const KoopaTensorShape &other) const {
  return !(*this == other);
}

KoopaTensorError::KoopaTensorError(const std::string &message)
    : std::runtime_error(message) {}

bool isTensorDialectLine(const std::string &line) {
  return opt::startsWith(opt::trim(line), kTensorPrefix);
}

void parseTensorDialectLine(KoopaTensorInfo &info, const std::string &raw_line) {
  std::string line = opt::trim(raw_line);
  require(opt::startsWith(line, kTensorPrefix),
          "invalid tensor dialect line: " + raw_line);
  line = opt::trim(line.substr(kTensorPrefix.size()));
  size_t eq = line.find(" = ");
  size_t colon = line.rfind(" : ");
  if (eq == std::string::npos) {
    colon = line.rfind(": ");
    require(colon != std::string::npos,
            "invalid tensor declaration: " + line);
    std::string value = opt::trim(line.substr(0, colon));
    info.shapes[value] = parseShape(line.substr(colon + 1));
    return;
  }
  require(colon != std::string::npos && colon > eq,
          "invalid tensor op declaration: " + line);
  KoopaTensorOp operation;
  operation.result = opt::trim(line.substr(0, eq));
  std::vector<std::string> parts =
      opt::splitWhitespace(opt::trim(line.substr(eq + 3, colon - eq - 3)));
  require(!parts.empty(), "missing tensor op kind");
  operation.kind = parseKind(parts[0]);
  std::string operands_text = opt::trim(line.substr(eq + 3 + parts[0].size(),
                                                   colon - eq - 3 -
                                                       parts[0].size()));
  operation.operands = splitCommaList(operands_text);
  operation.shape = parseShape(line.substr(colon + 3));
  verifyTensorOperation(info, operation);
  info.shapes[operation.result] = operation.shape;
  info.operations.push_back(operation);
}

std::string formatTensorInfo(const KoopaTensorInfo &info) {
  std::set<std::string> operation_results;
  for (const KoopaTensorOp &operation : info.operations) {
    operation_results.insert(operation.result);
  }

  std::ostringstream output;
  bool first = true;
  auto writeLine = [&](const std::string &line) {
    if (!first) {
      output << '\n';
    }
    first = false;
    output << line;
  };

  for (const auto &[value, shape] : info.shapes) {
    if (operation_results.find(value) == operation_results.end()) {
      writeLine(formatTensorShapeDecl(value, shape));
    }
  }
  for (const KoopaTensorOp &operation : info.operations) {
    writeLine(formatTensorOpDecl(operation));
  }
  return output.str();
}

std::string formatTensorShapeDecl(const std::string &value,
                                  const KoopaTensorShape &shape) {
  return kTensorPrefix + value + ": " + shape.str();
}

std::string formatTensorOpDecl(const KoopaTensorOp &operation) {
  return kTensorPrefix + operation.result + " = " +
         tensorOpKindName(operation.kind) + " " +
         joinOperands(operation.operands) + " : " +
         operation.shape.str();
}

bool isTensorMatmulKind(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::Matmul:
  case KoopaTensorOpKind::MatmulTa:
  case KoopaTensorOpKind::MatmulTb:
  case KoopaTensorOpKind::MatmulTaTb:
  case KoopaTensorOpKind::MatmulBias:
  case KoopaTensorOpKind::MatmulBiasTa:
  case KoopaTensorOpKind::MatmulBiasTb:
  case KoopaTensorOpKind::MatmulBiasTaTb:
  case KoopaTensorOpKind::MatmulBiasRelu:
  case KoopaTensorOpKind::MatmulBiasReluTa:
  case KoopaTensorOpKind::MatmulBiasReluTb:
  case KoopaTensorOpKind::MatmulBiasReluTaTb:
  case KoopaTensorOpKind::MatmulRelu:
  case KoopaTensorOpKind::MatmulReluTa:
  case KoopaTensorOpKind::MatmulReluTb:
  case KoopaTensorOpKind::MatmulReluTaTb:
    return true;
  default:
    return false;
  }
}

bool tensorMatmulHasBias(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::MatmulBias:
  case KoopaTensorOpKind::MatmulBiasTa:
  case KoopaTensorOpKind::MatmulBiasTb:
  case KoopaTensorOpKind::MatmulBiasTaTb:
  case KoopaTensorOpKind::MatmulBiasRelu:
  case KoopaTensorOpKind::MatmulBiasReluTa:
  case KoopaTensorOpKind::MatmulBiasReluTb:
  case KoopaTensorOpKind::MatmulBiasReluTaTb:
    return true;
  default:
    return false;
  }
}

bool tensorMatmulHasRelu(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::MatmulBiasRelu:
  case KoopaTensorOpKind::MatmulBiasReluTa:
  case KoopaTensorOpKind::MatmulBiasReluTb:
  case KoopaTensorOpKind::MatmulBiasReluTaTb:
  case KoopaTensorOpKind::MatmulRelu:
  case KoopaTensorOpKind::MatmulReluTa:
  case KoopaTensorOpKind::MatmulReluTb:
  case KoopaTensorOpKind::MatmulReluTaTb:
    return true;
  default:
    return false;
  }
}

bool tensorMatmulTransposesLhs(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::MatmulTa:
  case KoopaTensorOpKind::MatmulTaTb:
  case KoopaTensorOpKind::MatmulBiasTa:
  case KoopaTensorOpKind::MatmulBiasTaTb:
  case KoopaTensorOpKind::MatmulBiasReluTa:
  case KoopaTensorOpKind::MatmulBiasReluTaTb:
  case KoopaTensorOpKind::MatmulReluTa:
  case KoopaTensorOpKind::MatmulReluTaTb:
    return true;
  default:
    return false;
  }
}

bool tensorMatmulTransposesRhs(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::MatmulTb:
  case KoopaTensorOpKind::MatmulTaTb:
  case KoopaTensorOpKind::MatmulBiasTb:
  case KoopaTensorOpKind::MatmulBiasTaTb:
  case KoopaTensorOpKind::MatmulBiasReluTb:
  case KoopaTensorOpKind::MatmulBiasReluTaTb:
  case KoopaTensorOpKind::MatmulReluTb:
  case KoopaTensorOpKind::MatmulReluTaTb:
    return true;
  default:
    return false;
  }
}

KoopaTensorOpKind tensorMatmulKind(bool has_bias, bool has_relu,
                                   bool transpose_lhs, bool transpose_rhs) {
  if (has_bias && has_relu) {
    if (transpose_lhs && transpose_rhs) {
      return KoopaTensorOpKind::MatmulBiasReluTaTb;
    }
    if (transpose_lhs) return KoopaTensorOpKind::MatmulBiasReluTa;
    if (transpose_rhs) return KoopaTensorOpKind::MatmulBiasReluTb;
    return KoopaTensorOpKind::MatmulBiasRelu;
  }
  if (has_bias) {
    if (transpose_lhs && transpose_rhs) {
      return KoopaTensorOpKind::MatmulBiasTaTb;
    }
    if (transpose_lhs) return KoopaTensorOpKind::MatmulBiasTa;
    if (transpose_rhs) return KoopaTensorOpKind::MatmulBiasTb;
    return KoopaTensorOpKind::MatmulBias;
  }
  if (has_relu) {
    if (transpose_lhs && transpose_rhs) {
      return KoopaTensorOpKind::MatmulReluTaTb;
    }
    if (transpose_lhs) return KoopaTensorOpKind::MatmulReluTa;
    if (transpose_rhs) return KoopaTensorOpKind::MatmulReluTb;
    return KoopaTensorOpKind::MatmulRelu;
  }
  if (transpose_lhs && transpose_rhs) return KoopaTensorOpKind::MatmulTaTb;
  if (transpose_lhs) return KoopaTensorOpKind::MatmulTa;
  if (transpose_rhs) return KoopaTensorOpKind::MatmulTb;
  return KoopaTensorOpKind::Matmul;
}

std::string tensorOpKindName(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::Add:
    return "add";
  case KoopaTensorOpKind::AddRelu:
    return "add_relu";
  case KoopaTensorOpKind::Broadcast:
    return "broadcast";
  case KoopaTensorOpKind::Matmul:
    return "matmul";
  case KoopaTensorOpKind::MatmulTa:
    return "matmul_ta";
  case KoopaTensorOpKind::MatmulTb:
    return "matmul_tb";
  case KoopaTensorOpKind::MatmulTaTb:
    return "matmul_ta_tb";
  case KoopaTensorOpKind::MatmulBias:
    return "matmul_bias";
  case KoopaTensorOpKind::MatmulBiasTa:
    return "matmul_bias_ta";
  case KoopaTensorOpKind::MatmulBiasTb:
    return "matmul_bias_tb";
  case KoopaTensorOpKind::MatmulBiasTaTb:
    return "matmul_bias_ta_tb";
  case KoopaTensorOpKind::MatmulBiasRelu:
    return "matmul_bias_relu";
  case KoopaTensorOpKind::MatmulBiasReluTa:
    return "matmul_bias_relu_ta";
  case KoopaTensorOpKind::MatmulBiasReluTb:
    return "matmul_bias_relu_tb";
  case KoopaTensorOpKind::MatmulBiasReluTaTb:
    return "matmul_bias_relu_ta_tb";
  case KoopaTensorOpKind::MatmulRelu:
    return "matmul_relu";
  case KoopaTensorOpKind::MatmulReluTa:
    return "matmul_relu_ta";
  case KoopaTensorOpKind::MatmulReluTb:
    return "matmul_relu_tb";
  case KoopaTensorOpKind::MatmulReluTaTb:
    return "matmul_relu_ta_tb";
  case KoopaTensorOpKind::Relu:
    return "relu";
  case KoopaTensorOpKind::Reshape:
    return "reshape";
  case KoopaTensorOpKind::Transpose:
    return "transpose";
  }
  throw KoopaTensorError("unknown tensor op kind");
}

} // namespace compiler::ir
