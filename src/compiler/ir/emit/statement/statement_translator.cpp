#include "compiler/ir/emit/statement/statement_translator.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/sdt/production_rules.h"

#include <utility>

namespace compiler::ir {

StatementTranslator::StatementTranslator(StatementEmitContext &context)
    : context_(context) {
  translator_.setDefaultRule(
      [this](const compiler::parser::ParseNode &node,
             const sdt::SyntaxDirectedTranslator &) -> sdt::AttributeSet {
        return returnedAttribute(emitByShape(node));
      });

  registerRule("Stmt", {"LVal", "ASSIGN", "Exp", "SEMICOLON"},
               &StatementTranslator::emitAssignmentStatement);
  registerRule("Stmt", {"Exp", "SEMICOLON"},
               &StatementTranslator::emitExpressionStatement);
  registerRule("Stmt", {"SEMICOLON"},
               &StatementTranslator::emitEmptyStatement);
  registerRule("Stmt", {"Block"}, &StatementTranslator::emitBlockStatement);
  registerRule("Stmt", {"IF", "LPAREN", "Exp", "RPAREN", "Stmt", "ElseOpt"},
               &StatementTranslator::emitIfStatement);
  registerRule("Stmt", {"WHILE", "LPAREN", "Exp", "RPAREN", "Stmt"},
               &StatementTranslator::emitWhileStatement);
  registerRule("Stmt", {"BREAK", "SEMICOLON"},
               &StatementTranslator::emitBreakStatement);
  registerRule("Stmt", {"CONTINUE", "SEMICOLON"},
               &StatementTranslator::emitContinueStatement);
  registerRule("Stmt", {"RETURN", "ReturnExpOpt", "SEMICOLON"},
               &StatementTranslator::emitReturnStatement);
}

bool StatementTranslator::translate(
    const compiler::parser::ParseNode &node) const {
  sdt::AttributeSet attributes = translator_.translate(node);
  if (!attributes.has("returned")) {
    throw IrError("SDT statement action did not produce a result: " +
                  node.symbol);
  }
  return attributes.get<bool>("returned");
}

sdt::AttributeSet StatementTranslator::returnedAttribute(bool returned) const {
  sdt::AttributeSet attributes;
  attributes.set("returned", returned);
  return attributes;
}

bool StatementTranslator::emitByShape(
    const compiler::parser::ParseNode &node) const {
  if (node.symbol != "Stmt" || node.children.empty()) {
    throw IrError("unsupported statement in Koopa generation: " + node.symbol);
  }

  const std::string &first = node.children[0]->symbol;
  if (first == "RETURN") {
    return emitReturnStatement(node);
  }
  if (first == "LVal" && node.children.size() >= 3 &&
      node.children[1]->symbol == "ASSIGN") {
    return emitAssignmentStatement(node);
  }
  if (first == "IF") {
    return emitIfStatement(node);
  }
  if (first == "WHILE") {
    return emitWhileStatement(node);
  }
  if (first == "BREAK") {
    return emitBreakStatement(node);
  }
  if (first == "CONTINUE") {
    return emitContinueStatement(node);
  }
  if (first == "Block") {
    return emitBlockStatement(node);
  }
  if (first == "Exp") {
    return emitExpressionStatement(node);
  }
  if (first == "SEMICOLON") {
    return emitEmptyStatement(node);
  }

  throw IrError("unsupported statement in Koopa generation: " + first);
}

void StatementTranslator::registerRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    StatementRule rule) {
  sdt::registerProductionRule(
      translator_, lhs, rhs, [this, rule = std::move(rule)](
                                const compiler::parser::ParseNode &node,
                                const sdt::SyntaxDirectedTranslator &) {
        return returnedAttribute(rule(node));
      });
}

void StatementTranslator::registerRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    StatementRuleMethod rule) {
  registerRule(lhs, rhs, [this, rule](const compiler::parser::ParseNode &node) {
    return (this->*rule)(node);
  });
}

} // namespace compiler::ir
