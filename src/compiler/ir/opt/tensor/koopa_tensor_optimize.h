#pragma once

#include "compiler/ir/tensor/koopa_tensor_ir.h"

namespace compiler::ir::opt {

KoopaTensorInfo optimizeTensorInfo(const KoopaTensorInfo &info);

} // namespace compiler::ir::opt
