#include "compiler/ir/sdt/sdt.h"
#include "compiler/ir/sdt/production_rules.h"
#include "tests/test_framework.h"

#include <memory>

using namespace compiler;

TEST_CASE(sdt_translator_dispatches_rules_by_production_id) {
  parser::ParseNode parent("Exp");
  parent.production_id = 10;
  parent.children.push_back(std::make_unique<parser::ParseNode>("INT_CONST", "42"));

  ir::sdt::SyntaxDirectedTranslator translator;
  translator.registerRule(10, [](const parser::ParseNode &node,
                                   const ir::sdt::SyntaxDirectedTranslator &t) {
    ir::sdt::AttributeSet child = t.translate(*node.children[0]);
    ir::sdt::AttributeSet result;
    result.set<long long>("value", std::stoll(child.get<std::string>("lexeme")));
    return result;
  });

  ir::sdt::AttributeSet result = translator.translate(parent);

  EXPECT_EQ(result.get<long long>("value"), 42);
}

TEST_CASE(production_rules_registers_rules_by_production_shape) {
  parser::ParseNode number("Number");
  number.production_id = ir::sdt::findProductionId("Number", {"INT_CONST"});
  number.children.push_back(std::make_unique<parser::ParseNode>("INT_CONST", "7"));

  ir::sdt::SyntaxDirectedTranslator translator;
  ir::sdt::registerProductionRule(
      translator, "Number", {"INT_CONST"},
      [](const parser::ParseNode &node,
         const ir::sdt::SyntaxDirectedTranslator &t) {
        ir::sdt::AttributeSet child = t.translate(*node.children[0]);
        ir::sdt::AttributeSet result;
        result.set<long long>("value", std::stoll(child.get<std::string>("lexeme")));
        return result;
      });

  ir::sdt::AttributeSet result = translator.translate(number);

  EXPECT_EQ(result.get<long long>("value"), 7);
}
