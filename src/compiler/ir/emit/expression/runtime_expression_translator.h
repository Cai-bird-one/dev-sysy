#pragma once

#include "compiler/ir/model/ir_model.h"
#include "compiler/ir/sdt/sdt.h"
#include "compiler/parser/parser.h"

#include <functional>
#include <initializer_list>
#include <string>

namespace compiler::ir {

class RuntimeExpressionContext {
public:
  virtual ~RuntimeExpressionContext() = default;

  virtual Value makeConstant(long long value) const = 0;
  virtual Value emitLVal(const compiler::parser::ParseNode &node) = 0;
  virtual Value emitCall(const compiler::parser::ParseNode &node) = 0;
  virtual Value emitBinary(const std::string &op, Value lhs, Value rhs) = 0;
  virtual Value emitBoolean(Value value) = 0;
  virtual Value emitShortCircuitTail(const compiler::parser::ParseNode &node,
                                     Value lhs, bool is_or) = 0;
};

class RuntimeExpressionTranslator {
public:
  explicit RuntimeExpressionTranslator(RuntimeExpressionContext &context);

  Value translate(const compiler::parser::ParseNode &node) const;

private:
  using ExpressionRule =
      std::function<Value(const compiler::parser::ParseNode &)>;
  using ExpressionRuleMethod =
      Value (RuntimeExpressionTranslator::*)(
          const compiler::parser::ParseNode &) const;

  sdt::AttributeSet valueAttribute(Value value) const;
  Value emitNumber(const compiler::parser::ParseNode &node) const;
  Value emitParenthesizedExpression(
      const compiler::parser::ParseNode &node) const;
  Value emitLValExpression(const compiler::parser::ParseNode &node) const;
  Value emitCallExpression(const compiler::parser::ParseNode &node) const;
  Value emitUnaryExpression(const compiler::parser::ParseNode &node) const;
  Value emitBinaryExpression(const compiler::parser::ParseNode &node,
                             const std::string &tail_symbol) const;
  Value translateSingleChild(const compiler::parser::ParseNode &node) const;
  Value translateTail(const compiler::parser::ParseNode &node, Value lhs) const;
  void registerRule(const std::string &lhs,
                    std::initializer_list<std::string> rhs,
                    ExpressionRule rule);
  void registerRule(const std::string &lhs,
                    std::initializer_list<std::string> rhs,
                    ExpressionRuleMethod rule);
  void registerBinaryRule(const std::string &lhs,
                          std::initializer_list<std::string> rhs,
                          const std::string &tail_symbol);

  RuntimeExpressionContext &context_;
  sdt::SyntaxDirectedTranslator translator_;
};

} // namespace compiler::ir
