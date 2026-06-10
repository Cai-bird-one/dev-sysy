#include "compiler/ir/emit/expression/runtime_expression_translator.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/sdt/production_rules.h"

#include <utility>

namespace compiler::ir {

RuntimeExpressionTranslator::RuntimeExpressionTranslator(
    RuntimeExpressionContext &context)
    : context_(context) {
  translator_.setDefaultRule(
      [](const compiler::parser::ParseNode &node,
         const sdt::SyntaxDirectedTranslator &) -> sdt::AttributeSet {
        throw IrError(
            "unsupported expression production in Koopa generation: " +
            node.symbol);
      });

  registerRule("Number", {"INT_CONST"},
               &RuntimeExpressionTranslator::emitNumber);
  registerRule("PrimaryExp", {"LPAREN", "Exp", "RPAREN"},
               &RuntimeExpressionTranslator::emitParenthesizedExpression);
  registerRule("PrimaryExp", {"LVal"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("PrimaryExp", {"Number"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("UnaryExp", {"IDENT", "LPAREN", "FuncRParamsOpt", "RPAREN"},
               &RuntimeExpressionTranslator::emitCallExpression);
  registerRule("UnaryExp", {"PrimaryExp"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("UnaryExp", {"UnaryOp", "UnaryExp"},
               &RuntimeExpressionTranslator::emitUnaryExpression);
  registerRule("Exp", {"LOrExp"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("ConstExp", {"Exp"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("ConstInitVal", {"ConstExp"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("InitVal", {"Exp"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerRule("ReturnExpOpt", {"Exp"},
               &RuntimeExpressionTranslator::translateSingleChild);
  registerBinaryRule("MulExp", {"UnaryExp", "MulExpTail"}, "MulExpTail");
  registerBinaryRule("AddExp", {"MulExp", "AddExpTail"}, "AddExpTail");
  registerBinaryRule("RelExp", {"AddExp", "RelExpTail"}, "RelExpTail");
  registerBinaryRule("EqExp", {"RelExp", "EqExpTail"}, "EqExpTail");
  registerBinaryRule("LAndExp", {"EqExp", "LAndExpTail"}, "LAndExpTail");
  registerBinaryRule("LOrExp", {"LAndExp", "LOrExpTail"}, "LOrExpTail");
  registerRule("LVal", {"IDENT", "LValArrayDims"},
               &RuntimeExpressionTranslator::emitLValExpression);
}

Value RuntimeExpressionTranslator::translate(
    const compiler::parser::ParseNode &node) const {
  sdt::AttributeSet attributes = translator_.translate(node);
  if (!attributes.has("value")) {
    throw IrError("SDT expression action did not produce a value: " +
                  node.symbol);
  }
  return attributes.get<Value>("value");
}

sdt::AttributeSet RuntimeExpressionTranslator::valueAttribute(Value value) const {
  sdt::AttributeSet attributes;
  attributes.set("value", std::move(value));
  return attributes;
}

void RuntimeExpressionTranslator::registerRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    ExpressionRule rule) {
  int production_id = sdt::findProductionId(lhs, rhs);
  translator_.registerRule(
      production_id,
      [this, rule = std::move(rule)](
          const compiler::parser::ParseNode &node,
             const sdt::SyntaxDirectedTranslator &) {
        return valueAttribute(rule(node));
      });
}

void RuntimeExpressionTranslator::registerRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    ExpressionRuleMethod rule) {
  registerRule(lhs, rhs, [this, rule](const compiler::parser::ParseNode &node) {
    return (this->*rule)(node);
  });
}

void RuntimeExpressionTranslator::registerBinaryRule(
    const std::string &lhs, std::initializer_list<std::string> rhs,
    const std::string &tail_symbol) {
  registerRule(lhs, rhs, [this, tail_symbol](
                             const compiler::parser::ParseNode &node) {
    return emitBinaryExpression(node, tail_symbol);
  });
}

} // namespace compiler::ir
