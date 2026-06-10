#pragma once

#include "compiler/ir/model/ir_model.h"
#include "compiler/ir/sdt/sdt.h"
#include "compiler/parser/parser.h"

#include <functional>
#include <initializer_list>
#include <string>

namespace compiler::ir {

struct StatementLoopLabels {
  std::string break_label;
  std::string continue_label;
};

class StatementEmitContext {
public:
  virtual ~StatementEmitContext() = default;

  virtual Value emitExpression(const compiler::parser::ParseNode &node) = 0;
  virtual Value emitBoolean(Value value) = 0;
  virtual std::string
  lookupVariablePointer(const compiler::parser::ParseNode &lval) = 0;
  virtual void emitInstruction(std::string instruction) = 0;
  virtual void emitLabel(const std::string &label) = 0;
  virtual std::string newLabel(const std::string &prefix) = 0;
  virtual bool emitBlock(const compiler::parser::ParseNode &node,
                         bool create_scope) = 0;
  virtual const std::string &returnType() const = 0;
  virtual const std::string &functionName() const = 0;
  virtual void markReturned() = 0;
  virtual bool hasLoop() const = 0;
  virtual StatementLoopLabels currentLoop() const = 0;
  virtual void pushLoop(StatementLoopLabels labels) = 0;
  virtual void popLoop() = 0;
};

class StatementTranslator {
public:
  explicit StatementTranslator(StatementEmitContext &context);

  bool translate(const compiler::parser::ParseNode &node) const;

private:
  using StatementRule =
      std::function<bool(const compiler::parser::ParseNode &)>;
  using StatementRuleMethod =
      bool (StatementTranslator::*)(const compiler::parser::ParseNode &) const;

  sdt::AttributeSet returnedAttribute(bool returned) const;
  bool emitByShape(const compiler::parser::ParseNode &node) const;
  void registerRule(const std::string &lhs,
                    std::initializer_list<std::string> rhs,
                    StatementRule rule);
  void registerRule(const std::string &lhs,
                    std::initializer_list<std::string> rhs,
                    StatementRuleMethod rule);

  bool emitReturnStatement(const compiler::parser::ParseNode &node) const;
  bool emitAssignmentStatement(const compiler::parser::ParseNode &node) const;
  bool emitIfStatement(const compiler::parser::ParseNode &node) const;
  bool emitWhileStatement(const compiler::parser::ParseNode &node) const;
  bool emitBreakStatement(const compiler::parser::ParseNode &node) const;
  bool emitContinueStatement(const compiler::parser::ParseNode &node) const;
  bool emitBlockStatement(const compiler::parser::ParseNode &node) const;
  bool emitExpressionStatement(const compiler::parser::ParseNode &node) const;
  bool emitEmptyStatement(const compiler::parser::ParseNode &node) const;

  StatementEmitContext &context_;
  sdt::SyntaxDirectedTranslator translator_;
};

} // namespace compiler::ir
