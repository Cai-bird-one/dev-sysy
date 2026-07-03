#pragma once

#include "compiler/ir/tensor/koopa_tensor_ir.h"

namespace compiler::ir {

void verifyTensorOperation(const KoopaTensorInfo &info,
                           const KoopaTensorOp &operation);

} // namespace compiler::ir
