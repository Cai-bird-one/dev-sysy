#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <map>
#include <set>
#include <sstream>

namespace compiler::ir::opt {
namespace {

struct FunctionStorage {
  std::set<std::string> values;
  std::set<std::string> pointer_params;
};

class MatmulEmitter {
public:
  MatmulEmitter(const KoopaTensorInfo &tensor, const KoopaTensorOp &operation,
                const std::set<std::string> &global_values,
                const std::set<std::string> &pointer_params, int id)
      : tensor_(tensor), operation_(operation), global_values_(global_values),
        pointer_params_(pointer_params),
        prefix_("%tensor_mm" + std::to_string(id)) {}

  std::vector<std::string> emit() {
    std::vector<std::string> lines;
    emitAllocations(lines);
    if (shouldUnrollInner()) {
      emitJump(lines, label("unroll_i_start"));
      emitComputeUnrolled(lines);
    } else if (shouldPackRhs()) {
      emitJump(lines, label("pack_j_start"));
      emitPackRhs(lines);
      emitCompute(lines);
    } else {
      emitJump(lines, label("compute_i_start"));
      emitCompute(lines);
    }
    emitLabel(lines, "done");
    return lines;
  }

private:
  const KoopaTensorInfo &tensor_;
  const KoopaTensorOp &operation_;
  const std::set<std::string> &global_values_;
  const std::set<std::string> &pointer_params_;
  std::string prefix_;
  int value_id_ = 0;

  std::string i() const { return prefix_ + "_i"; }
  std::string j() const { return prefix_ + "_j"; }
  std::string k() const { return prefix_ + "_k"; }
  std::string sum() const { return prefix_ + "_sum"; }
  std::string rhsPack() const { return prefix_ + "_rhs_pack"; }

  std::string label(const std::string &name) const {
    return prefix_ + "_" + name;
  }

  std::string fresh(const std::string &name) {
    return prefix_ + "_v" + std::to_string(value_id_++) + "_" + name;
  }

  KoopaTensorShape shapeOf(const std::string &value) const {
    auto found = tensor_.shapes.find(value);
    if (found == tensor_.shapes.end()) {
      throw KoopaTensorError("unknown tensor value in lowering: " + value);
    }
    return found->second;
  }

  bool usesGetptrForFirstIndex(const std::string &base) const {
    return pointer_params_.find(base) != pointer_params_.end() ||
           (startsWith(base, "@") && global_values_.find(base) ==
                                     global_values_.end());
  }

  void emitLabel(std::vector<std::string> &lines,
                 const std::string &name) const {
    lines.push_back(label(name) + ":");
  }

  void emitJump(std::vector<std::string> &lines,
                const std::string &target) const {
    lines.push_back("jump " + target);
  }

  void emitBranch(std::vector<std::string> &lines, const std::string &cond,
                  const std::string &then_label,
                  const std::string &else_label) const {
    lines.push_back("br " + cond + ", " + then_label + ", " + else_label);
  }

  std::string emitLoad(std::vector<std::string> &lines,
                       const std::string &pointer,
                       const std::string &name) {
    std::string value = fresh(name);
    lines.push_back(value + " = load " + pointer);
    return value;
  }

  void emitStore(std::vector<std::string> &lines, const std::string &value,
                 const std::string &pointer) const {
    lines.push_back("store " + value + ", " + pointer);
  }

  std::string emitAdd(std::vector<std::string> &lines, const std::string &lhs,
                      const std::string &rhs, const std::string &name) {
    std::string value = fresh(name);
    lines.push_back(value + " = add " + lhs + ", " + rhs);
    return value;
  }

  std::string emitMul(std::vector<std::string> &lines, const std::string &lhs,
                      const std::string &rhs, const std::string &name) {
    std::string value = fresh(name);
    lines.push_back(value + " = mul " + lhs + ", " + rhs);
    return value;
  }

  std::string emitLt(std::vector<std::string> &lines, const std::string &lhs,
                     int rhs, const std::string &name) {
    std::string value = fresh(name);
    lines.push_back(value + " = lt " + lhs + ", " + std::to_string(rhs));
    return value;
  }

  std::string emitLt(std::vector<std::string> &lines, const std::string &lhs,
                     const std::string &rhs, const std::string &name) {
    std::string value = fresh(name);
    lines.push_back(value + " = lt " + lhs + ", " + rhs);
    return value;
  }

  std::string emitMatrixElementPointer(std::vector<std::string> &lines,
                                       const std::string &base,
                                       const std::string &row,
                                       const std::string &col,
                                       bool transposed,
                                       const std::string &name) {
    std::string physical_row = transposed ? col : row;
    std::string physical_col = transposed ? row : col;
    std::string row_ptr = fresh(name + "_row");
    std::string first_op =
        usesGetptrForFirstIndex(base) ? "getptr " : "getelemptr ";
    lines.push_back(row_ptr + " = " + first_op + base + ", " + physical_row);
    std::string elem_ptr = fresh(name + "_elem");
    lines.push_back(elem_ptr + " = getelemptr " + row_ptr + ", " +
                    physical_col);
    return elem_ptr;
  }

  std::string emitTensorElementPointer(std::vector<std::string> &lines,
                                       const std::string &base,
                                       const KoopaTensorShape &shape,
                                       const std::string &row,
                                       const std::string &col,
                                       const std::string &name) {
    if (shape.dims.size() == 1) {
      std::string index = shape.dims[0] == 1 ? "0" : col;
      std::string elem_ptr = fresh(name + "_elem");
      std::string op = usesGetptrForFirstIndex(base) ? "getptr " :
                                                       "getelemptr ";
      lines.push_back(elem_ptr + " = " + op + base + ", " + index);
      return elem_ptr;
    }

    std::string bias_row = shape.dims[0] == 1 ? "0" : row;
    std::string bias_col = shape.dims[1] == 1 ? "0" : col;
    return emitMatrixElementPointer(lines, base, bias_row, bias_col, false,
                                    name);
  }

  std::string emitOutputPointer(std::vector<std::string> &lines,
                                const std::string &row,
                                const std::string &col) {
    return emitMatrixElementPointer(lines, operation_.result, row, col, false,
                                    "out");
  }

  void emitIncrement(std::vector<std::string> &lines,
                     const std::string &slot,
                     const std::string &next_label) {
    std::string value = emitLoad(lines, slot, "inc_old");
    std::string next = emitAdd(lines, value, "1", "inc_next");
    emitStore(lines, next, slot);
    emitJump(lines, next_label);
  }

  void emitAllocations(std::vector<std::string> &lines) {
    lines.push_back(i() + " = alloc i32");
    lines.push_back(j() + " = alloc i32");
    lines.push_back(k() + " = alloc i32");
    lines.push_back(sum() + " = alloc i32");
    if (shouldPackRhs()) {
      int cols = operation_.shape.dims[1];
      int inner = innerDimension();
      lines.push_back(rhsPack() + " = alloc [[i32, " +
                      std::to_string(inner) + "], " +
                      std::to_string(cols) + "]");
    }
  }

  int innerDimension() const {
    KoopaTensorShape lhs_shape = shapeOf(operation_.operands[0]);
    return tensorMatmulTransposesLhs(operation_.kind) ? lhs_shape.dims[0]
                                                     : lhs_shape.dims[1];
  }

  bool shouldPackRhs() const {
    int cols = operation_.shape.dims[1];
    int inner = innerDimension();
    return !shouldUnrollInner() && !tensorMatmulTransposesRhs(operation_.kind) &&
           cols * inner >= 4096;
  }

  bool shouldUnrollInner() const {
    int inner = innerDimension();
    return inner <= 128;
  }

  std::string emitRhsElementPointer(std::vector<std::string> &lines,
                                    const std::string &logical_row,
                                    const std::string &logical_col) {
    if (shouldPackRhs()) {
      return emitMatrixElementPointer(lines, rhsPack(), logical_col,
                                      logical_row, false, "rhs_pack");
    }
    return emitMatrixElementPointer(
        lines, operation_.operands[1], logical_row, logical_col,
        tensorMatmulTransposesRhs(operation_.kind), "rhs");
  }

  void emitPackRhs(std::vector<std::string> &lines) {
    int cols = operation_.shape.dims[1];
    int inner = innerDimension();

    emitLabel(lines, "pack_j_start");
    emitStore(lines, "0", j());
    emitJump(lines, label("pack_j_cond"));

    emitLabel(lines, "pack_j_cond");
    std::string jv = emitLoad(lines, j(), "pack_j");
    std::string jcmp = emitLt(lines, jv, cols, "pack_j_cmp");
    emitBranch(lines, jcmp, label("pack_k_start"), label("compute_i_start"));

    emitLabel(lines, "pack_k_start");
    emitStore(lines, "0", k());
    emitJump(lines, label("pack_k_cond"));

    emitLabel(lines, "pack_k_cond");
    std::string kv = emitLoad(lines, k(), "pack_k");
    std::string kcmp = emitLt(lines, kv, inner, "pack_k_cmp");
    emitBranch(lines, kcmp, label("pack_body"), label("pack_j_next"));

    emitLabel(lines, "pack_body");
    jv = emitLoad(lines, j(), "pack_body_j");
    kv = emitLoad(lines, k(), "pack_body_k");
    std::string rhs_ptr = emitMatrixElementPointer(
        lines, operation_.operands[1], kv, jv,
        tensorMatmulTransposesRhs(operation_.kind), "rhs");
    std::string pack_ptr =
        emitMatrixElementPointer(lines, rhsPack(), jv, kv, false, "pack");
    std::string value = emitLoad(lines, rhs_ptr, "rhs");
    emitStore(lines, value, pack_ptr);
    emitIncrement(lines, k(), label("pack_k_cond"));

    emitLabel(lines, "pack_j_next");
    emitIncrement(lines, j(), label("pack_j_cond"));
  }

  void emitComputeUnrolled(std::vector<std::string> &lines) {
    int rows = operation_.shape.dims[0];
    int cols = operation_.shape.dims[1];
    int inner = innerDimension();

    emitLabel(lines, "unroll_i_start");
    emitStore(lines, "0", i());
    emitJump(lines, label("unroll_i_cond"));

    emitLabel(lines, "unroll_i_cond");
    std::string iv = emitLoad(lines, i(), "unroll_i");
    std::string icmp = emitLt(lines, iv, rows, "unroll_i_cmp");
    emitBranch(lines, icmp, label("unroll_j_start"), label("done"));

    emitLabel(lines, "unroll_j_start");
    emitStore(lines, "0", j());
    emitJump(lines, label("unroll_j_cond"));

    emitLabel(lines, "unroll_j_cond");
    std::string jv = emitLoad(lines, j(), "unroll_j");
    std::string jcmp = emitLt(lines, jv, cols, "unroll_j_cmp");
    emitBranch(lines, jcmp, label("unroll_cell"), label("unroll_i_next"));

    emitLabel(lines, "unroll_cell");
    iv = emitLoad(lines, i(), "cell_i");
    jv = emitLoad(lines, j(), "cell_j");
    std::string total = "0";
    if (tensorMatmulHasBias(operation_.kind)) {
      const std::string &bias = operation_.operands[2];
      KoopaTensorShape bias_shape = shapeOf(bias);
      std::string bias_ptr =
          emitTensorElementPointer(lines, bias, bias_shape, iv, jv, "bias");
      total = emitLoad(lines, bias_ptr, "bias");
    }

    for (int kk = 0; kk < inner; ++kk) {
      std::string index = std::to_string(kk);
      std::string lhs_ptr = emitMatrixElementPointer(
          lines, operation_.operands[0], iv, index,
          tensorMatmulTransposesLhs(operation_.kind), "lhs");
      std::string rhs_ptr = emitMatrixElementPointer(
          lines, operation_.operands[1], index, jv,
          tensorMatmulTransposesRhs(operation_.kind), "rhs");
      std::string lhs_value = emitLoad(lines, lhs_ptr, "lhs");
      std::string rhs_value = emitLoad(lines, rhs_ptr, "rhs");
      std::string product = emitMul(lines, lhs_value, rhs_value, "product");
      total = total == "0" ? product : emitAdd(lines, total, product, "sum");
    }

    std::string out_ptr = emitOutputPointer(lines, iv, jv);
    if (tensorMatmulHasRelu(operation_.kind)) {
      std::string negative = emitLt(lines, total, "0", "relu_negative");
      emitBranch(lines, negative, label("unroll_store_zero"),
                 label("unroll_store_sum"));
    } else {
      emitStore(lines, total, out_ptr);
      emitJump(lines, label("unroll_j_next"));
    }

    if (tensorMatmulHasRelu(operation_.kind)) {
      emitLabel(lines, "unroll_store_zero");
      emitStore(lines, "0", out_ptr);
      emitJump(lines, label("unroll_j_next"));

      emitLabel(lines, "unroll_store_sum");
      emitStore(lines, total, out_ptr);
      emitJump(lines, label("unroll_j_next"));
    }

    emitLabel(lines, "unroll_j_next");
    emitIncrement(lines, j(), label("unroll_j_cond"));

    emitLabel(lines, "unroll_i_next");
    emitIncrement(lines, i(), label("unroll_i_cond"));
  }

  void emitCompute(std::vector<std::string> &lines) {
    int rows = operation_.shape.dims[0];
    int cols = operation_.shape.dims[1];
    int inner = innerDimension();

    emitLabel(lines, "compute_i_start");
    emitStore(lines, "0", i());
    emitJump(lines, label("compute_i_cond"));

    emitLabel(lines, "compute_i_cond");
    std::string iv = emitLoad(lines, i(), "compute_i");
    std::string icmp = emitLt(lines, iv, rows, "compute_i_cmp");
    emitBranch(lines, icmp, label("compute_j_start"), label("done"));

    emitLabel(lines, "compute_j_start");
    emitStore(lines, "0", j());
    emitJump(lines, label("compute_j_cond"));

    emitLabel(lines, "compute_j_cond");
    std::string jv = emitLoad(lines, j(), "compute_j");
    std::string jcmp = emitLt(lines, jv, cols, "compute_j_cmp");
    emitBranch(lines, jcmp, label("compute_cell_start"),
               label("compute_i_next"));

    emitLabel(lines, "compute_cell_start");
    iv = emitLoad(lines, i(), "cell_i");
    jv = emitLoad(lines, j(), "cell_j");
    std::string initial = "0";
    if (tensorMatmulHasBias(operation_.kind)) {
      const std::string &bias = operation_.operands[2];
      KoopaTensorShape bias_shape = shapeOf(bias);
      std::string bias_ptr =
          emitTensorElementPointer(lines, bias, bias_shape, iv, jv, "bias");
      initial = emitLoad(lines, bias_ptr, "bias");
    }
    emitStore(lines, initial, sum());
    emitStore(lines, "0", k());
    emitJump(lines, label("compute_k_cond"));

    emitLabel(lines, "compute_k_cond");
    std::string kv = emitLoad(lines, k(), "compute_k");
    std::string kcmp = emitLt(lines, kv, inner, "compute_k_cmp");
    emitBranch(lines, kcmp, label("compute_k_body"),
               label("compute_store"));

    emitLabel(lines, "compute_k_body");
    iv = emitLoad(lines, i(), "body_i");
    jv = emitLoad(lines, j(), "body_j");
    kv = emitLoad(lines, k(), "body_k");
    std::string lhs_ptr = emitMatrixElementPointer(
        lines, operation_.operands[0], iv, kv,
        tensorMatmulTransposesLhs(operation_.kind), "lhs");
    std::string rhs_ptr = emitRhsElementPointer(lines, kv, jv);
    std::string lhs_value = emitLoad(lines, lhs_ptr, "lhs");
    std::string rhs_value = emitLoad(lines, rhs_ptr, "rhs");
    std::string old_sum = emitLoad(lines, sum(), "sum_old");
    std::string product = emitMul(lines, lhs_value, rhs_value, "product");
    std::string new_sum = emitAdd(lines, old_sum, product, "sum");
    emitStore(lines, new_sum, sum());
    emitIncrement(lines, k(), label("compute_k_cond"));

    emitLabel(lines, "compute_store");
    iv = emitLoad(lines, i(), "store_i");
    jv = emitLoad(lines, j(), "store_j");
    std::string out_ptr = emitOutputPointer(lines, iv, jv);
    std::string total = emitLoad(lines, sum(), "sum_final");
    if (tensorMatmulHasRelu(operation_.kind)) {
      std::string negative = emitLt(lines, total, "0", "relu_negative");
      emitBranch(lines, negative, label("compute_store_zero"),
                 label("compute_store_sum"));
    } else {
      emitStore(lines, total, out_ptr);
      emitJump(lines, label("compute_j_next"));
    }

    if (tensorMatmulHasRelu(operation_.kind)) {
      emitLabel(lines, "compute_store_zero");
      emitStore(lines, "0", out_ptr);
      emitJump(lines, label("compute_j_next"));

      emitLabel(lines, "compute_store_sum");
      emitStore(lines, total, out_ptr);
      emitJump(lines, label("compute_j_next"));
    }

    emitLabel(lines, "compute_j_next");
    emitIncrement(lines, j(), label("compute_j_cond"));

    emitLabel(lines, "compute_i_next");
    emitIncrement(lines, i(), label("compute_i_cond"));
  }

  void emitInitialization(std::vector<std::string> &lines) {
    int rows = operation_.shape.dims[0];
    int cols = operation_.shape.dims[1];

    emitLabel(lines, "init_i_start");
    emitStore(lines, "0", i());
    emitJump(lines, label("init_i_cond"));

    emitLabel(lines, "init_i_cond");
    std::string iv = emitLoad(lines, i(), "init_i");
    std::string icmp = emitLt(lines, iv, rows, "init_i_cmp");
    emitBranch(lines, icmp, label("init_j_start"), label("acc_i_start"));

    emitLabel(lines, "init_j_start");
    emitStore(lines, "0", j());
    emitJump(lines, label("init_j_cond"));

    emitLabel(lines, "init_j_cond");
    std::string jv = emitLoad(lines, j(), "init_j");
    std::string jcmp = emitLt(lines, jv, cols, "init_j_cmp");
    emitBranch(lines, jcmp, label("init_body"), label("init_i_next"));

    emitLabel(lines, "init_body");
    iv = emitLoad(lines, i(), "init_body_i");
    jv = emitLoad(lines, j(), "init_body_j");
    std::string out_ptr = emitOutputPointer(lines, iv, jv);
    std::string initial = "0";
    if (tensorMatmulHasBias(operation_.kind)) {
      const std::string &bias = operation_.operands[2];
      KoopaTensorShape bias_shape = shapeOf(bias);
      std::string bias_ptr =
          emitTensorElementPointer(lines, bias, bias_shape, iv, jv, "bias");
      initial = emitLoad(lines, bias_ptr, "bias");
    }
    emitStore(lines, initial, out_ptr);
    emitIncrement(lines, j(), label("init_j_cond"));

    emitLabel(lines, "init_i_next");
    emitIncrement(lines, i(), label("init_i_cond"));
  }

  void emitAccumulation(std::vector<std::string> &lines) {
    int rows = operation_.shape.dims[0];
    int cols = operation_.shape.dims[1];
    KoopaTensorShape lhs_shape = shapeOf(operation_.operands[0]);
    int inner = tensorMatmulTransposesLhs(operation_.kind) ? lhs_shape.dims[0]
                                                          : lhs_shape.dims[1];

    emitLabel(lines, "acc_i_start");
    emitStore(lines, "0", i());
    emitJump(lines, label("acc_i_cond"));

    emitLabel(lines, "acc_i_cond");
    std::string iv = emitLoad(lines, i(), "acc_i");
    std::string icmp = emitLt(lines, iv, rows, "acc_i_cmp");
    emitBranch(lines, icmp, label("acc_k_start"),
               tensorMatmulHasRelu(operation_.kind) ? label("relu_i_start")
                                                    : label("done"));

    emitLabel(lines, "acc_k_start");
    emitStore(lines, "0", k());
    emitJump(lines, label("acc_k_cond"));

    emitLabel(lines, "acc_k_cond");
    std::string kv = emitLoad(lines, k(), "acc_k");
    std::string kcmp = emitLt(lines, kv, inner, "acc_k_cmp");
    emitBranch(lines, kcmp, label("acc_j_start"), label("acc_i_next"));

    emitLabel(lines, "acc_j_start");
    emitStore(lines, "0", j());
    emitJump(lines, label("acc_j_cond"));

    emitLabel(lines, "acc_j_cond");
    std::string jv = emitLoad(lines, j(), "acc_j");
    std::string jcmp = emitLt(lines, jv, cols, "acc_j_cmp");
    emitBranch(lines, jcmp, label("acc_body"), label("acc_k_next"));

    emitLabel(lines, "acc_body");
    iv = emitLoad(lines, i(), "acc_body_i");
    kv = emitLoad(lines, k(), "acc_body_k");
    jv = emitLoad(lines, j(), "acc_body_j");
    const std::string &lhs = operation_.operands[0];
    const std::string &rhs = operation_.operands[1];
    std::string lhs_ptr = emitMatrixElementPointer(
        lines, lhs, iv, kv, tensorMatmulTransposesLhs(operation_.kind), "lhs");
    std::string rhs_ptr = emitMatrixElementPointer(
        lines, rhs, kv, jv, tensorMatmulTransposesRhs(operation_.kind), "rhs");
    std::string out_ptr = emitOutputPointer(lines, iv, jv);
    std::string lhs_value = emitLoad(lines, lhs_ptr, "lhs");
    std::string rhs_value = emitLoad(lines, rhs_ptr, "rhs");
    std::string old_value = emitLoad(lines, out_ptr, "out_old");
    std::string product = emitMul(lines, lhs_value, rhs_value, "product");
    std::string sum = emitAdd(lines, old_value, product, "sum");
    emitStore(lines, sum, out_ptr);
    emitIncrement(lines, j(), label("acc_j_cond"));

    emitLabel(lines, "acc_k_next");
    emitIncrement(lines, k(), label("acc_k_cond"));

    emitLabel(lines, "acc_i_next");
    emitIncrement(lines, i(), label("acc_i_cond"));
  }

  void emitRelu(std::vector<std::string> &lines) {
    int rows = operation_.shape.dims[0];
    int cols = operation_.shape.dims[1];

    emitLabel(lines, "relu_i_start");
    emitStore(lines, "0", i());
    emitJump(lines, label("relu_i_cond"));

    emitLabel(lines, "relu_i_cond");
    std::string iv = emitLoad(lines, i(), "relu_i");
    std::string icmp = emitLt(lines, iv, rows, "relu_i_cmp");
    emitBranch(lines, icmp, label("relu_j_start"), label("done"));

    emitLabel(lines, "relu_j_start");
    emitStore(lines, "0", j());
    emitJump(lines, label("relu_j_cond"));

    emitLabel(lines, "relu_j_cond");
    std::string jv = emitLoad(lines, j(), "relu_j");
    std::string jcmp = emitLt(lines, jv, cols, "relu_j_cmp");
    emitBranch(lines, jcmp, label("relu_body"), label("relu_i_next"));

    emitLabel(lines, "relu_body");
    iv = emitLoad(lines, i(), "relu_body_i");
    jv = emitLoad(lines, j(), "relu_body_j");
    std::string out_ptr = emitOutputPointer(lines, iv, jv);
    std::string value = emitLoad(lines, out_ptr, "relu_value");
    std::string negative = emitLt(lines, value, "0", "relu_negative");
    emitBranch(lines, negative, label("relu_store_zero"),
               label("relu_j_next"));

    emitLabel(lines, "relu_store_zero");
    emitStore(lines, "0", out_ptr);
    emitJump(lines, label("relu_j_next"));

    emitLabel(lines, "relu_j_next");
    emitIncrement(lines, j(), label("relu_j_cond"));

    emitLabel(lines, "relu_i_next");
    emitIncrement(lines, i(), label("relu_i_cond"));
  }
};

std::vector<std::string> splitTopLevelCommaList(const std::string &text) {
  std::vector<std::string> result;
  size_t begin = 0;
  int depth = 0;
  for (size_t i = 0; i <= text.size(); ++i) {
    bool at_end = i == text.size();
    char ch = at_end ? ',' : text[i];
    if (!at_end && (ch == '[' || ch == '(' || ch == '{')) {
      ++depth;
    } else if (!at_end && (ch == ']' || ch == ')' || ch == '}')) {
      --depth;
    }
    if ((at_end || ch == ',') && depth == 0) {
      std::string item = trim(text.substr(begin, i - begin));
      if (!item.empty()) {
        result.push_back(item);
      }
      begin = i + 1;
    }
  }
  return result;
}

std::set<std::string> collectPointerParams(const std::string &header) {
  std::set<std::string> params;
  size_t left = header.find('(');
  size_t right = header.find(')', left);
  if (left == std::string::npos || right == std::string::npos || right < left) {
    return params;
  }
  std::string text = trim(header.substr(left + 1, right - left - 1));
  for (const std::string &param : splitTopLevelCommaList(text)) {
    size_t colon = param.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string name = trim(param.substr(0, colon));
    std::string type = trim(param.substr(colon + 1));
    if (startsWith(type, "*")) {
      params.insert(name);
    }
  }
  return params;
}

std::set<std::string> collectGlobalValues(const IrModule &module) {
  std::set<std::string> globals;
  for (const std::string &line : module.globals) {
    if (!startsWith(line, "global ")) {
      continue;
    }
    size_t begin = std::string("global ").size();
    size_t end = line.find(" = ", begin);
    if (end != std::string::npos) {
      globals.insert(line.substr(begin, end - begin));
    }
  }
  return globals;
}

FunctionStorage collectFunctionStorage(const IrFunction &function,
                                       const std::set<std::string> &globals) {
  FunctionStorage storage;
  storage.values = globals;
  storage.pointer_params = collectPointerParams(function.header);
  storage.values.insert(storage.pointer_params.begin(),
                        storage.pointer_params.end());
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && assignment.op == "alloc") {
      storage.values.insert(assignment.result);
    }
  }
  return storage;
}

std::string tensorOpResult(const std::string &line) {
  std::string text = trim(line);
  if (!isTensorDialectLine(text)) {
    return "";
  }
  text = trim(text.substr(std::string("tensor ").size()));
  size_t eq = text.find(" = ");
  if (eq == std::string::npos) {
    return "";
  }
  return trim(text.substr(0, eq));
}

bool hasStorageForOperation(const KoopaTensorOp &operation,
                            const FunctionStorage &storage) {
  if (!isTensorMatmulKind(operation.kind) || operation.shape.dims.size() != 2 ||
      operation.operands.size() < 2) {
    return false;
  }
  if (storage.values.find(operation.result) == storage.values.end()) {
    return false;
  }
  for (const std::string &operand : operation.operands) {
    if (storage.values.find(operand) == storage.values.end()) {
      return false;
    }
  }
  return true;
}

} // namespace

PassResult TensorMatmulLoweringPass::run(IrModule &module) {
  std::map<std::string, KoopaTensorOp> matmul_operations;
  for (const KoopaTensorOp &operation : module.tensor.operations) {
    if (isTensorMatmulKind(operation.kind)) {
      matmul_operations[operation.result] = operation;
    }
  }
  if (matmul_operations.empty()) {
    return {};
  }

  std::set<std::string> global_values = collectGlobalValues(module);
  std::set<std::string> lowered_results;
  bool changed = false;
  int lowering_id = 0;

  for (IrFunction &function : module.functions) {
    FunctionStorage storage = collectFunctionStorage(function, global_values);
    std::vector<std::string> lowered;
    for (const std::string &line : function.instructions) {
      std::string result = tensorOpResult(line);
      if (result.empty()) {
        lowered.push_back(line);
        continue;
      }

      auto found = matmul_operations.find(result);
      if (found != matmul_operations.end() &&
          hasStorageForOperation(found->second, storage)) {
        MatmulEmitter emitter(module.tensor, found->second, global_values,
                              storage.pointer_params, lowering_id++);
        std::vector<std::string> emitted = emitter.emit();
        lowered.insert(lowered.end(), emitted.begin(), emitted.end());
        lowered_results.insert(result);
      }
      changed = true;
    }
    function.instructions = std::move(lowered);
  }

  if (!lowered_results.empty()) {
    std::vector<KoopaTensorOp> remaining;
    for (const KoopaTensorOp &operation : module.tensor.operations) {
      if (lowered_results.find(operation.result) == lowered_results.end()) {
        remaining.push_back(operation);
      }
    }
    module.tensor.operations = std::move(remaining);
  }

  return {changed};
}

} // namespace compiler::ir::opt
