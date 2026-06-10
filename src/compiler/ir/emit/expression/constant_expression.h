#pragma once

#include "compiler/ir/model/ir_model.h"
#include "compiler/parser/parser.h"

#include <functional>
#include <string>
#include <utility>

namespace compiler::ir {

class ConstantExpressionEvaluator {
public:
  using LookupSymbol = std::function<const Symbol &(const std::string &)>;

  explicit ConstantExpressionEvaluator(LookupSymbol lookup_symbol);

  Value evaluate(const compiler::parser::ParseNode &node) const;

private:
  Value evaluateBinaryTail(const compiler::parser::ParseNode &node) const;
  Value evaluateTail(const compiler::parser::ParseNode &node, Value lhs) const;

  LookupSymbol lookup_symbol_;
};

} // namespace compiler::ir
