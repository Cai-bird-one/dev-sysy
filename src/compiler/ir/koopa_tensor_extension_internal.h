#pragma once

#include "compiler/ir/koopa_tensor_extension.h"

namespace compiler::ir {

void verifyTensorOperation(const KoopaTensorInfo &info,
                           const KoopaTensorOp &operation);

} // namespace compiler::ir
