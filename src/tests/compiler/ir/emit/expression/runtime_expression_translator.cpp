#include "compiler/ir/emit/expression/constant_expression.h"
#include "compiler/ir/emit/expression/runtime_expression_translator.h"
#include "compiler/ir/model/ir_model.h"
#include "compiler/ir/sdt/production_rules.h"
#include "compiler/ir/util/ir_utils.h"
#include "compiler/parser/parser.h"
#include "tests/test_framework.h"

#include <map>
#include <memory>
#include <string>

using namespace compiler;

namespace {

std::unique_ptr<parser::ParseNode> terminal(const std::string &symbol,
                                            const std::string &lexeme = "") {
  return std::make_unique<parser::ParseNode>(symbol, lexeme);
}

std::unique_ptr<parser::ParseNode>
node(const std::string &symbol, std::initializer_list<std::string> rhs) {
  auto result = std::make_unique<parser::ParseNode>(symbol);
  result->production_id = ir::sdt::findProductionId(symbol, rhs);
  return result;
}

std::unique_ptr<parser::ParseNode> number(const std::string &value) {
  auto result = node("Number", {"INT_CONST"});
  result->children.push_back(terminal("INT_CONST", value));
  return result;
}

std::unique_ptr<parser::ParseNode> primaryNumber(const std::string &value) {
  auto result = node("PrimaryExp", {"Number"});
  result->children.push_back(number(value));
  return result;
}

std::unique_ptr<parser::ParseNode> unaryNumber(const std::string &value) {
  auto result = node("UnaryExp", {"PrimaryExp"});
  result->children.push_back(primaryNumber(value));
  return result;
}

class TestExpressionContext : public ir::RuntimeExpressionContext {
public:
  ir::Value makeConstant(long long value) const override {
    return ir::Value{true, value, ir::toOperand(value)};
  }

  ir::Value emitLVal(const parser::ParseNode &) override {
    return makeConstant(11);
  }

  ir::Value emitCall(const parser::ParseNode &) override {
    return makeConstant(13);
  }

  ir::Value emitBinary(const std::string &op, ir::Value lhs,
                       ir::Value rhs) override {
    return makeConstant(ir::foldBinary(op, lhs.const_value, rhs.const_value));
  }

  ir::Value emitBoolean(ir::Value value) override {
    return makeConstant(value.const_value != 0 ? 1 : 0);
  }

  ir::Value emitShortCircuitTail(const parser::ParseNode &node, ir::Value lhs,
                                 bool is_or) override {
    if (node.children.empty()) {
      return emitBoolean(lhs);
    }
    bool lhs_true = lhs.const_value != 0;
    if ((is_or && lhs_true) || (!is_or && !lhs_true)) {
      return makeConstant(lhs_true ? 1 : 0);
    }
    return makeConstant(1);
  }
};

} // namespace

TEST_CASE(runtime_expression_translator_uses_sdt_rules_for_binary_exp) {
  auto mul_tail = node("MulExpTail", {"STAR", "UnaryExp", "MulExpTail"});
  mul_tail->children.push_back(terminal("STAR", "*"));
  mul_tail->children.push_back(unaryNumber("3"));
  mul_tail->children.push_back(node("MulExpTail", {}));

  auto mul = node("MulExp", {"UnaryExp", "MulExpTail"});
  mul->children.push_back(unaryNumber("2"));
  mul->children.push_back(std::move(mul_tail));

  auto add_tail = node("AddExpTail", {"PLUS", "MulExp", "AddExpTail"});
  add_tail->children.push_back(terminal("PLUS", "+"));
  add_tail->children.push_back(std::move(mul));
  add_tail->children.push_back(node("AddExpTail", {}));

  auto lhs_mul = node("MulExp", {"UnaryExp", "MulExpTail"});
  lhs_mul->children.push_back(unaryNumber("1"));
  lhs_mul->children.push_back(node("MulExpTail", {}));

  auto add = node("AddExp", {"MulExp", "AddExpTail"});
  add->children.push_back(std::move(lhs_mul));
  add->children.push_back(std::move(add_tail));

  TestExpressionContext context;
  ir::RuntimeExpressionTranslator translator(context);
  ir::Value value = translator.translate(*add);

  EXPECT_TRUE(value.constant);
  EXPECT_EQ(value.const_value, 7);
}

TEST_CASE(constant_expression_evaluator_uses_symbol_lookup) {
  std::map<std::string, ir::Symbol> symbols;
  symbols["answer"] = ir::Symbol{ir::SymbolKind::Constant, 42, "", {}, false,
                                 false};

  parser::ParseNode lval("LVal");
  lval.children.push_back(terminal("IDENT", "answer"));

  ir::ConstantExpressionEvaluator evaluator(
      [&symbols](const std::string &name) -> const ir::Symbol & {
        return symbols.at(name);
      });
  ir::Value value = evaluator.evaluate(lval);

  EXPECT_TRUE(value.constant);
  EXPECT_EQ(value.const_value, 42);
}
