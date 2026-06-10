#include "compiler/ir/emit/statement/statement_translator.h"

#include "compiler/ir/ast/parse_node_utils.h"
#include "compiler/ir/koopa_generator.h"

namespace compiler::ir {

bool StatementTranslator::emitReturnStatement(
    const compiler::parser::ParseNode &node) const {
  const compiler::parser::ParseNode *return_exp =
      findDirectChild(node, "ReturnExpOpt");
  if (return_exp != nullptr && !return_exp->children.empty()) {
    if (context_.returnType() == "void") {
      throw IrError("void function cannot return a value: " +
                    context_.functionName());
    }
    context_.emitInstruction("ret " +
                             context_.emitExpression(*return_exp).operand);
  } else {
    const compiler::parser::ParseNode *old_exp = findDirectChild(node, "Exp");
    if (old_exp != nullptr) {
      if (context_.returnType() == "void") {
        throw IrError("void function cannot return a value: " +
                      context_.functionName());
      }
      context_.emitInstruction("ret " +
                               context_.emitExpression(*old_exp).operand);
    } else {
      context_.emitInstruction(context_.returnType() == "void" ? "ret"
                                                               : "ret 0");
    }
  }
  context_.markReturned();
  return true;
}

bool StatementTranslator::emitAssignmentStatement(
    const compiler::parser::ParseNode &node) const {
  std::string pointer = context_.lookupVariablePointer(*node.children[0]);
  Value value = context_.emitExpression(*node.children[2]);
  context_.emitInstruction("store " + value.operand + ", " + pointer);
  return false;
}

bool StatementTranslator::emitIfStatement(
    const compiler::parser::ParseNode &node) const {
  if (node.children.size() != 6) {
    throw IrError("invalid if statement node");
  }
  Value condition =
      context_.emitBoolean(context_.emitExpression(*node.children[2]));
  std::string then_label = context_.newLabel("if_then");
  std::string else_label = context_.newLabel("if_else");
  std::string end_label = context_.newLabel("if_end");
  const compiler::parser::ParseNode &else_opt = *node.children[5];
  bool has_else = !else_opt.children.empty();

  context_.emitInstruction("br " + condition.operand + ", " + then_label +
                           ", " + (has_else ? else_label : end_label));

  context_.emitLabel(then_label);
  bool then_returned = translate(*node.children[4]);
  if (!then_returned) {
    context_.emitInstruction("jump " + end_label);
  }

  bool else_returned = false;
  if (has_else) {
    context_.emitLabel(else_label);
    if (else_opt.children.size() != 2 ||
        else_opt.children[0]->symbol != "ELSE" ||
        else_opt.children[1]->symbol != "Stmt") {
      throw IrError("invalid else statement node");
    }
    else_returned = translate(*else_opt.children[1]);
    if (!else_returned) {
      context_.emitInstruction("jump " + end_label);
    }
  }

  if (!then_returned || !has_else || !else_returned) {
    context_.emitLabel(end_label);
  }
  return has_else && then_returned && else_returned;
}

bool StatementTranslator::emitWhileStatement(
    const compiler::parser::ParseNode &node) const {
  if (node.children.size() != 5) {
    throw IrError("invalid while statement node");
  }

  std::string entry_label = context_.newLabel("while_entry");
  std::string body_label = context_.newLabel("while_body");
  std::string end_label = context_.newLabel("while_end");

  context_.emitInstruction("jump " + entry_label);
  context_.emitLabel(entry_label);
  Value condition =
      context_.emitBoolean(context_.emitExpression(*node.children[2]));
  context_.emitInstruction("br " + condition.operand + ", " + body_label +
                           ", " + end_label);

  context_.emitLabel(body_label);
  context_.pushLoop(StatementLoopLabels{end_label, entry_label});
  bool body_terminated = translate(*node.children[4]);
  context_.popLoop();
  if (!body_terminated) {
    context_.emitInstruction("jump " + entry_label);
  }

  context_.emitLabel(end_label);
  return false;
}

bool StatementTranslator::emitBreakStatement(
    const compiler::parser::ParseNode &) const {
  if (!context_.hasLoop()) {
    throw IrError("break statement outside loop");
  }
  context_.emitInstruction("jump " + context_.currentLoop().break_label);
  return true;
}

bool StatementTranslator::emitContinueStatement(
    const compiler::parser::ParseNode &) const {
  if (!context_.hasLoop()) {
    throw IrError("continue statement outside loop");
  }
  context_.emitInstruction("jump " + context_.currentLoop().continue_label);
  return true;
}

bool StatementTranslator::emitBlockStatement(
    const compiler::parser::ParseNode &node) const {
  return context_.emitBlock(*node.children[0], true);
}

bool StatementTranslator::emitExpressionStatement(
    const compiler::parser::ParseNode &node) const {
  context_.emitExpression(*node.children[0]);
  return false;
}

bool StatementTranslator::emitEmptyStatement(
    const compiler::parser::ParseNode &) const {
  return false;
}

} // namespace compiler::ir
