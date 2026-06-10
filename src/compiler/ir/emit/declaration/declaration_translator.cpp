#include "compiler/ir/emit/declaration/declaration_translator.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/sdt/production_rules.h"

#include <utility>

namespace compiler::ir {

DeclarationTranslator::DeclarationTranslator(DeclarationContext &context)
    : context_(context) {
  translator_.setDefaultRule(
      [this](const compiler::parser::ParseNode &node,
             const sdt::SyntaxDirectedTranslator &) {
        walkChildren(node);
        return doneAttribute();
      });

  registerRule("Decl", {"ConstDecl"}, &DeclarationTranslator::walkChildren);
  registerRule("Decl", {"VarDecl"}, &DeclarationTranslator::walkChildren);
  registerRule(
      "ConstDecl", {"CONST", "BType", "ConstDef", "ConstDefList", "SEMICOLON"},
      &DeclarationTranslator::walkChildren);
  registerRule("ConstDefList", {"COMMA", "ConstDef", "ConstDefList"},
               &DeclarationTranslator::walkChildren);
  registerRule("ConstDefList", {}, &DeclarationTranslator::ignoreNode);
  registerRule("ConstDef",
               {"IDENT", "ConstArrayDims", "ASSIGN", "ConstInitVal"},
               &DeclarationTranslator::emitConstDefinitionNode);
  registerRule("VarDecl", {"BType", "VarDef", "VarDefList", "SEMICOLON"},
               &DeclarationTranslator::walkChildren);
  registerRule("VarDefList", {"COMMA", "VarDef", "VarDefList"},
               &DeclarationTranslator::walkChildren);
  registerRule("VarDefList", {}, &DeclarationTranslator::ignoreNode);
  registerRule("VarDef", {"IDENT", "ConstArrayDims", "VarDefInitOpt"},
               &DeclarationTranslator::emitVarDefinitionNode);
}

void DeclarationTranslator::translate(
    const compiler::parser::ParseNode &node) const {
  sdt::AttributeSet attributes = translator_.translate(node);
  if (!attributes.has("done")) {
    throw IrError("SDT declaration action did not finish: " + node.symbol);
  }
}

sdt::AttributeSet DeclarationTranslator::doneAttribute() const {
  sdt::AttributeSet attributes;
  attributes.set("done", true);
  return attributes;
}

void DeclarationTranslator::walkChildren(
    const compiler::parser::ParseNode &node) const {
  for (const auto &child : node.children) {
    translate(*child);
  }
}

void DeclarationTranslator::emitConstDefinitionNode(
    const compiler::parser::ParseNode &node) const {
  context_.emitConstDefinition(node);
}

void DeclarationTranslator::emitVarDefinitionNode(
    const compiler::parser::ParseNode &node) const {
  context_.emitVarDefinition(node);
}

void DeclarationTranslator::ignoreNode(
    const compiler::parser::ParseNode &) const {}

void DeclarationTranslator::registerRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    DeclarationRule rule) {
  sdt::registerProductionRule(
      translator_, lhs, rhs, [this, rule = std::move(rule)](
                                const compiler::parser::ParseNode &node,
                                const sdt::SyntaxDirectedTranslator &) {
        rule(node);
        return doneAttribute();
      });
}

void DeclarationTranslator::registerRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    DeclarationRuleMethod rule) {
  registerRule(lhs, rhs, [this, rule](const compiler::parser::ParseNode &node) {
    (this->*rule)(node);
  });
}

} // namespace compiler::ir
