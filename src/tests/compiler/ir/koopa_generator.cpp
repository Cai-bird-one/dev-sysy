#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

TEST_CASE(koopa_generator_emits_minimal_function) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  std::istringstream input("int main() { return 0; }");
  std::unique_ptr<parser::ParseNode> comp_unit = parser.parse(lexer.tokenize(input));

  ir::KoopaGenerator generator;
  std::string koopa = generator.generate(*comp_unit);

  EXPECT_EQ(koopa, "fun @main(): i32 {\n%entry:\n  ret 0\n}\n");
}

TEST_CASE(koopa_generator_emits_from_manual_ast) {
  parser::ParseNode comp_unit("CompUnit");
  auto exp = std::make_unique<parser::ParseNode>("Exp");
  auto unary_exp = std::make_unique<parser::ParseNode>("UnaryExp");
  auto primary_exp = std::make_unique<parser::ParseNode>("PrimaryExp");
  auto number = std::make_unique<parser::ParseNode>("Number");
  number->children.push_back(std::make_unique<parser::ParseNode>("INT_CONST", "0"));
  primary_exp->children.push_back(std::move(number));
  unary_exp->children.push_back(std::move(primary_exp));
  exp->children.push_back(std::move(unary_exp));
  auto stmt = std::make_unique<parser::ParseNode>("Stmt");
  stmt->children.push_back(std::make_unique<parser::ParseNode>("RETURN", "return"));
  stmt->children.push_back(std::move(exp));
  stmt->children.push_back(std::make_unique<parser::ParseNode>("SEMICOLON", ";"));
  auto block = std::make_unique<parser::ParseNode>("Block");
  block->children.push_back(std::move(stmt));
  auto func_def = std::make_unique<parser::ParseNode>("FuncDef");
  func_def->children.push_back(std::make_unique<parser::ParseNode>("IDENT", "main"));
  func_def->children.push_back(std::move(block));
  comp_unit.children.push_back(std::move(func_def));

  ir::KoopaGenerator generator;
  std::string koopa = generator.generate(comp_unit);

  EXPECT_EQ(koopa, "fun @main(): i32 {\n%entry:\n  ret 0\n}\n");
}

TEST_CASE(koopa_generator_runs_after_lexer_and_parser) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main() { return 0; }");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_EQ(koopa, "fun @main(): i32 {\n%entry:\n  ret 0\n}\n");
}

TEST_CASE(koopa_generator_normalizes_integer_literals) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){return 0x2a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 42\n}\n");

  std::istringstream octal_input("int main(){return 052;}");
  ast = parser.parse(lexer.tokenize(octal_input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 42\n}\n");
}

TEST_CASE(koopa_generator_handles_unary_constant_expressions) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){return -+!0;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret -1\n}\n");

  std::istringstream paren_input("int main(){return (1);}");
  ast = parser.parse(lexer.tokenize(paren_input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 1\n}\n");
}

TEST_CASE(koopa_generator_handles_binary_constant_expressions) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){const int a=1+2*3,b=(a-1)%4;return b;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 2\n}\n");
}

TEST_CASE(koopa_generator_handles_global_declarations) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("const int a = 10, b = 5; int main(){return b;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  EXPECT_EQ(generator.generate(*ast), "fun @main(): i32 {\n%entry:\n  ret 5\n}\n");

  std::istringstream var_input("int g = 7, h; int main(){return g;}");
  ast = parser.parse(lexer.tokenize(var_input));
  EXPECT_EQ(generator.generate(*ast),
            "global @g = alloc i32, 7\n"
            "global @h = alloc i32, zeroinit\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  %0 = load @g\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_global_variable_store) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int g; int main(){g=3; return g;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "global @g = alloc i32, zeroinit\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  store 3, @g\n"
            "  %0 = load @g\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_local_variable_storage) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=1; a=a+2; return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 1, %a\n"
            "  %0 = load %a\n"
            "  %1 = add %0, 2\n"
            "  store %1, %a\n"
            "  %2 = load %a\n"
            "  ret %2\n"
            "}\n");
}

TEST_CASE(koopa_generator_initializes_unassigned_local_variables) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a; return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_allows_local_scope_to_shadow_globals) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int a=1; int main(){int a=2; return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "global @a = alloc i32, 1\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 2, %a\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_resolves_nested_block_scopes) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=1; {int a=2;} return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  %a_1 = alloc i32\n"
            "  store 1, %a\n"
            "  store 2, %a_1\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_uses_outer_scope_in_shadowing_initializer) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream global_input("int a=1; int main(){int a=a+1; return a;}");
  std::unique_ptr<parser::ParseNode> ast =
      parser.parse(lexer.tokenize(global_input));

  EXPECT_EQ(generator.generate(*ast),
            "global @a = alloc i32, 1\n"
            "\n"
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  %0 = load @a\n"
            "  %1 = add %0, 1\n"
            "  store %1, %a\n"
            "  %2 = load %a\n"
            "  ret %2\n"
            "}\n");

  std::istringstream local_input(
      "int main(){int a=1; {int a=a+1; return a;}}");
  ast = parser.parse(lexer.tokenize(local_input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  %a_1 = alloc i32\n"
            "  store 1, %a\n"
            "  %0 = load %a\n"
            "  %1 = add %0, 1\n"
            "  store %1, %a_1\n"
            "  %2 = load %a_1\n"
            "  ret %2\n"
            "}\n");
}

TEST_CASE(koopa_generator_booleanizes_runtime_logical_operands) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=2,b=4; return a&&b;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  std::string koopa = generator.generate(*ast);
  EXPECT_TRUE(koopa.find("br %1, %and_rhs_0, %and_end_1") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%and_rhs_0:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("%and_end_1:\n") != std::string::npos);

  std::istringstream or_input("int main(){int a=2,b=4; return a||b;}");
  ast = parser.parse(lexer.tokenize(or_input));

  koopa = generator.generate(*ast);
  EXPECT_TRUE(koopa.find("br %1, %or_end_1, %or_rhs_0") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%or_rhs_0:\n") != std::string::npos);
  EXPECT_TRUE(koopa.find("%or_end_1:\n") != std::string::npos);
}

TEST_CASE(koopa_generator_emits_if_else_branches) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("int main(){int a=0;if(1)a=1;else a=2;return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  br 1, %if_then_0, %if_else_1\n"
            "%if_then_0:\n"
            "  store 1, %a\n"
            "  jump %if_end_2\n"
            "%if_else_1:\n"
            "  store 2, %a\n"
            "  jump %if_end_2\n"
            "%if_end_2:\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_matches_else_to_nearest_if) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;if(1)if(0)a=1;else a=2;return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  br 1, %if_then_0, %if_end_2\n"
            "%if_then_0:\n"
            "  br 0, %if_then_3, %if_else_4\n"
            "%if_then_3:\n"
            "  store 1, %a\n"
            "  jump %if_end_5\n"
            "%if_else_4:\n"
            "  store 2, %a\n"
            "  jump %if_end_5\n"
            "%if_end_5:\n"
            "  jump %if_end_2\n"
            "%if_end_2:\n"
            "  %0 = load %a\n"
            "  ret %0\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_while_loop) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;while(a<3)a=a+1;return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));

  EXPECT_EQ(generator.generate(*ast),
            "fun @main(): i32 {\n%entry:\n"
            "  %a = alloc i32\n"
            "  store 0, %a\n"
            "  jump %while_entry_0\n"
            "%while_entry_0:\n"
            "  %0 = load %a\n"
            "  %1 = lt %0, 3\n"
            "  %2 = ne %1, 0\n"
            "  br %2, %while_body_1, %while_end_2\n"
            "%while_body_1:\n"
            "  %3 = load %a\n"
            "  %4 = add %3, 1\n"
            "  store %4, %a\n"
            "  jump %while_entry_0\n"
            "%while_end_2:\n"
            "  %5 = load %a\n"
            "  ret %5\n"
            "}\n");
}

TEST_CASE(koopa_generator_emits_break_and_continue) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;while(a<5){a=a+1;if(a==2)continue;if(a==4)break;}return a;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("jump %while_entry_0") != std::string::npos);
  EXPECT_TRUE(koopa.find("jump %while_end_2") != std::string::npos);
  EXPECT_TRUE(koopa.find("%while_end_2:\n") != std::string::npos);
}

TEST_CASE(koopa_generator_hoists_loop_body_allocs_to_entry) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int i=0;while(i<3){int x=i;i=i+1;if(i==2)continue;}return i;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("%i = alloc i32\n  %x = alloc i32") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%while_body_1:\n  %3 = load %i\n  store %3, %x") !=
              std::string::npos);
}

TEST_CASE(koopa_generator_terminates_unreachable_loop_exit_block) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int main(){int a=0;while(1){a=a+1;if(a==3)return a;continue;}}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("%while_end_2:\n  ret 0\n") != std::string::npos);
}

TEST_CASE(koopa_generator_emits_function_definitions_and_calls) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int add(int a,int b){return a+b;} int main(){return add(1,2);}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("fun @add(@add_a: i32, @add_b: i32): i32") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("store @add_a, %a\n  store @add_b, %b") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%0 = load %a\n  %1 = load %b\n  %2 = add %0, %1") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("%0 = call @add(1, 2)\n  ret %0") !=
              std::string::npos);
}

TEST_CASE(koopa_generator_emits_void_function_calls) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input("void sink(int a){return;} int main(){sink(1);return 0;}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("fun @sink(@sink_a: i32) {") != std::string::npos);
  EXPECT_TRUE(koopa.find("call @sink(1)\n  ret 0") != std::string::npos);
}

TEST_CASE(koopa_generator_avoids_global_and_parameter_name_collisions) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int n; int gcd(int m,int n){return n;} int main(){return gcd(1,2);}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("global @n = alloc i32, zeroinit") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("fun @gcd(@gcd_m: i32, @gcd_n: i32): i32") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("store @gcd_m, %m\n  store @gcd_n, %n") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("store @n, %n") == std::string::npos);
}

TEST_CASE(koopa_generator_rejects_duplicate_names_in_same_scope) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream local_input("int main(){int a; const int a=1; return a;}");
  std::unique_ptr<parser::ParseNode> ast =
      parser.parse(lexer.tokenize(local_input));

  bool rejected_local = false;
  try {
    generator.generate(*ast);
  } catch (const ir::IrError &) {
    rejected_local = true;
  }
  EXPECT_TRUE(rejected_local);

  std::istringstream global_input("int a; const int a=1; int main(){return 0;}");
  ast = parser.parse(lexer.tokenize(global_input));

  bool rejected_global = false;
  try {
    generator.generate(*ast);
  } catch (const ir::IrError &) {
    rejected_global = true;
  }
  EXPECT_TRUE(rejected_global);
}
