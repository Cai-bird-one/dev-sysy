#include "compiler/ir/emit/declaration/declaration_translator.h"
#include "compiler/ir/sdt/production_rules.h"
#include "compiler/parser/parser.h"
#include "tests/test_framework.h"

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

std::unique_ptr<parser::ParseNode> emptyConstArrayDims() {
  return node("ConstArrayDims", {});
}

std::unique_ptr<parser::ParseNode> emptyConstDefList() {
  return node("ConstDefList", {});
}

std::unique_ptr<parser::ParseNode> emptyVarDefList() {
  return node("VarDefList", {});
}

class TestDeclarationContext : public ir::DeclarationContext {
public:
  void emitConstDefinition(const parser::ParseNode &node) override {
    ++const_defs;
    last_name = node.children[0]->lexeme;
  }

  void emitVarDefinition(const parser::ParseNode &node) override {
    ++var_defs;
    last_name = node.children[0]->lexeme;
  }

  int const_defs = 0;
  int var_defs = 0;
  std::string last_name;
};

} // namespace

TEST_CASE(declaration_translator_walks_const_declaration) {
  auto const_def = node("ConstDef",
                        {"IDENT", "ConstArrayDims", "ASSIGN", "ConstInitVal"});
  const_def->children.push_back(terminal("IDENT", "answer"));
  const_def->children.push_back(emptyConstArrayDims());
  const_def->children.push_back(terminal("ASSIGN", "="));
  const_def->children.push_back(terminal("ConstInitVal"));

  auto const_decl = node("ConstDecl",
                         {"CONST", "BType", "ConstDef", "ConstDefList",
                          "SEMICOLON"});
  const_decl->children.push_back(terminal("CONST", "const"));
  const_decl->children.push_back(terminal("BType"));
  const_decl->children.push_back(std::move(const_def));
  const_decl->children.push_back(emptyConstDefList());
  const_decl->children.push_back(terminal("SEMICOLON", ";"));

  TestDeclarationContext context;
  ir::DeclarationTranslator translator(context);
  translator.translate(*const_decl);

  EXPECT_EQ(context.const_defs, 1);
  EXPECT_EQ(context.var_defs, 0);
  EXPECT_EQ(context.last_name, "answer");
}

TEST_CASE(declaration_translator_walks_var_declaration) {
  auto var_def = node("VarDef", {"IDENT", "ConstArrayDims", "VarDefInitOpt"});
  var_def->children.push_back(terminal("IDENT", "value"));
  var_def->children.push_back(emptyConstArrayDims());
  var_def->children.push_back(terminal("VarDefInitOpt"));

  auto var_decl = node("VarDecl", {"BType", "VarDef", "VarDefList",
                                   "SEMICOLON"});
  var_decl->children.push_back(terminal("BType"));
  var_decl->children.push_back(std::move(var_def));
  var_decl->children.push_back(emptyVarDefList());
  var_decl->children.push_back(terminal("SEMICOLON", ";"));

  TestDeclarationContext context;
  ir::DeclarationTranslator translator(context);
  translator.translate(*var_decl);

  EXPECT_EQ(context.const_defs, 0);
  EXPECT_EQ(context.var_defs, 1);
  EXPECT_EQ(context.last_name, "value");
}
