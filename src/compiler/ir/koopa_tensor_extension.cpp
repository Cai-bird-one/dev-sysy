#include "compiler/ir/koopa_tensor_extension.h"

#include "compiler/ir/koopa_tensor_extension_internal.h"
#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <set>
#include <sstream>

namespace compiler::ir {
namespace {

const std::string kPrefix = "// tensor ";

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
  if (text == "broadcast") return KoopaTensorOpKind::Broadcast;
  if (text == "matmul_bias_relu") return KoopaTensorOpKind::MatmulBiasRelu;
  if (text == "matmul") return KoopaTensorOpKind::Matmul;
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

KoopaTensorInfo KoopaTensorExtension::parse(const std::string &koopa_ir) const {
  KoopaTensorInfo info;
  std::istringstream input(koopa_ir);
  std::string line;
  while (std::getline(input, line)) {
    line = opt::trim(line);
    if (!opt::startsWith(line, kPrefix)) {
      continue;
    }
    line = opt::trim(line.substr(kPrefix.size()));
    size_t eq = line.find(" = ");
    size_t colon = line.rfind(" : ");
    if (eq == std::string::npos) {
      colon = line.rfind(": ");
      require(colon != std::string::npos, "invalid tensor annotation: " + line);
      std::string value = opt::trim(line.substr(0, colon));
      info.shapes[value] = parseShape(line.substr(colon + 1));
      continue;
    }
    require(colon != std::string::npos && colon > eq,
            "invalid tensor op annotation: " + line);
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
  return info;
}

void KoopaTensorExtension::verify(const std::string &koopa_ir) const {
  parse(koopa_ir);
}

KoopaTensorInfo
KoopaTensorExtension::optimize(const std::string &koopa_ir) const {
  return optimize(parse(koopa_ir));
}

std::string KoopaTensorExtension::format(const KoopaTensorInfo &info) const {
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
      writeLine(annotateShape(value, shape));
    }
  }
  for (const KoopaTensorOp &operation : info.operations) {
    writeLine(annotateOp(operation));
  }
  return output.str();
}

std::string
KoopaTensorExtension::annotateShape(const std::string &value,
                                    const KoopaTensorShape &shape) const {
  return kPrefix + value + ": " + shape.str();
}

std::string KoopaTensorExtension::annotateOp(
    const KoopaTensorOp &operation) const {
  return kPrefix + operation.result + " = " + tensorOpKindName(operation.kind) +
         " " + joinOperands(operation.operands) + " : " +
         operation.shape.str();
}

std::string tensorOpKindName(KoopaTensorOpKind kind) {
  switch (kind) {
  case KoopaTensorOpKind::Add:
    return "add";
  case KoopaTensorOpKind::Broadcast:
    return "broadcast";
  case KoopaTensorOpKind::MatmulBiasRelu:
    return "matmul_bias_relu";
  case KoopaTensorOpKind::Matmul:
    return "matmul";
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
