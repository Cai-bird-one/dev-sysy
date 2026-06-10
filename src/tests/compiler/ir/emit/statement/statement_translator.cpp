#include "compiler/ir/emit/statement/statement_translator.h"
#include "compiler/ir/sdt/production_rules.h"
#include "compiler/parser/parser.h"
#include "tests/test_framework.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

class TestStatementContext : public ir::StatementEmitContext {
public:
  ir::Value emitExpression(const parser::ParseNode &) override {
    last_action = "expr";
    return ir::Value{false, 0, "%value"};
  }

  ir::Value emitBoolean(ir::Value value) override { return value; }

  std::string lookupVariablePointer(const parser::ParseNode &) override {
    last_action = "lookup";
    return "@x";
  }

  void emitInstruction(std::string instruction) override {
    instructions.push_back(std::move(instruction));
  }

  void emitLabel(const std::string &label) override {
    instructions.push_back(label + ":");
  }

  std::string newLabel(const std::string &prefix) override {
    return "%" + prefix;
  }

  bool emitBlock(const parser::ParseNode &, bool) override {
    last_action = "block";
    return false;
  }

  const std::string &returnType() const override { return return_type; }

  const std::string &functionName() const override { return function_name; }

  void markReturned() override { returned_marked = true; }

  bool hasLoop() const override { return false; }

  ir::StatementLoopLabels currentLoop() const override {
    return ir::StatementLoopLabels{"%break", "%continue"};
  }

  void pushLoop(ir::StatementLoopLabels) override {}

  void popLoop() override {}

  std::string last_action;
  std::string return_type = "i32";
  std::string function_name = "@main";
  bool returned_marked = false;
  std::vector<std::string> instructions;
};

} // namespace

TEST_CASE(statement_translator_dispatches_return_statement) {
  auto stmt = node("Stmt", {"RETURN", "ReturnExpOpt", "SEMICOLON"});
  stmt->children.push_back(terminal("RETURN", "return"));
  stmt->children.push_back(node("ReturnExpOpt", {}));
  stmt->children.push_back(terminal("SEMICOLON", ";"));

  TestStatementContext context;
  ir::StatementTranslator translator(context);
  bool returned = translator.translate(*stmt);

  EXPECT_TRUE(returned);
  EXPECT_TRUE(context.returned_marked);
  EXPECT_EQ(context.instructions[0], "ret 0");
}

TEST_CASE(statement_translator_dispatches_assignment_statement) {
  auto stmt = node("Stmt", {"LVal", "ASSIGN", "Exp", "SEMICOLON"});
  stmt->children.push_back(terminal("LVal"));
  stmt->children.push_back(terminal("ASSIGN", "="));
  stmt->children.push_back(terminal("Exp"));
  stmt->children.push_back(terminal("SEMICOLON", ";"));

  TestStatementContext context;
  ir::StatementTranslator translator(context);
  bool returned = translator.translate(*stmt);

  EXPECT_TRUE(!returned);
  EXPECT_EQ(context.last_action, "expr");
  EXPECT_EQ(context.instructions[0], "store %value, @x");
}
