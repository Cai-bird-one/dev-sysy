#pragma once

#include "compiler/ir/sdt/sdt.h"
#include "compiler/parser/parser.h"

#include <functional>
#include <initializer_list>
#include <string>

namespace compiler::ir {

class DeclarationContext {
public:
  virtual ~DeclarationContext() = default;

  virtual void emitConstDefinition(const compiler::parser::ParseNode &node) = 0;
  virtual void emitVarDefinition(const compiler::parser::ParseNode &node) = 0;
};

class DeclarationTranslator {
public:
  explicit DeclarationTranslator(DeclarationContext &context);

  void translate(const compiler::parser::ParseNode &node) const;

private:
  using DeclarationRule =
      std::function<void(const compiler::parser::ParseNode &)>;
  using DeclarationRuleMethod =
      void (DeclarationTranslator::*)(const compiler::parser::ParseNode &) const;

  sdt::AttributeSet doneAttribute() const;
  void emitByShape(const compiler::parser::ParseNode &node) const;
  void walkChildren(const compiler::parser::ParseNode &node) const;
  void emitConstDefinitionNode(const compiler::parser::ParseNode &node) const;
  void emitVarDefinitionNode(const compiler::parser::ParseNode &node) const;
  void ignoreNode(const compiler::parser::ParseNode &node) const;
  void registerRule(const std::string &lhs,
                    std::initializer_list<std::string> rhs,
                    DeclarationRule rule);
  void registerRule(const std::string &lhs,
                    std::initializer_list<std::string> rhs,
                    DeclarationRuleMethod rule);

  DeclarationContext &context_;
  sdt::SyntaxDirectedTranslator translator_;
};

} // namespace compiler::ir
