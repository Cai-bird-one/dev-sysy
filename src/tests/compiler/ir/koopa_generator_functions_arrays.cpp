#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "tests/test_framework.h"

#include <memory>
#include <sstream>

using namespace compiler;

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

  std::istringstream input(
      "void sink(int a){return;} int main(){sink(1);return 0;}");
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

TEST_CASE(koopa_generator_emits_array_declarations_and_accesses) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int a[2][3]={{1,2},{3}};"
      "int main(){int b[2]={4};a[1][2]=b[0];return a[0][1];}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("global @a = alloc [[i32, 3], 2], "
                         "{{1, 2, 0}, {3, 0, 0}}") != std::string::npos);
  EXPECT_TRUE(koopa.find("%b = alloc [i32, 2]") != std::string::npos);
  EXPECT_TRUE(koopa.find("getelemptr @a, 1") != std::string::npos);
  EXPECT_TRUE(koopa.find("getelemptr %b, 0") != std::string::npos);
}

TEST_CASE(koopa_generator_decays_array_arguments) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "int sum(int a[], int b[][3]){return a[1]+b[1][2];}"
      "int main(){int x[4];int y[2][3];return sum(x,y);}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("fun @sum(@sum_a: *i32, @sum_b: *[i32, 3]): i32") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("getptr @sum_a, 1") != std::string::npos);
  EXPECT_TRUE(koopa.find("getptr @sum_b, 1") != std::string::npos);
  EXPECT_TRUE(koopa.find("call @sum(") != std::string::npos);
  EXPECT_TRUE(koopa.find("getelemptr %x, 0") != std::string::npos);
  EXPECT_TRUE(koopa.find("getelemptr %y, 0") != std::string::npos);
}

TEST_CASE(koopa_generator_uses_global_consts_in_array_parameter_types) {
  lexer::Lexer lexer = lexer::buildDefaultLexer();
  parser::Parser parser = parser::buildDefaultParser();
  ir::KoopaGenerator generator;

  std::istringstream input(
      "const int N=3; int sum(int a[][N]){return a[1][2];}"
      "int main(){int x[2][N];return sum(x);}");
  std::unique_ptr<parser::ParseNode> ast = parser.parse(lexer.tokenize(input));
  std::string koopa = generator.generate(*ast);

  EXPECT_TRUE(koopa.find("fun @sum(@sum_a: *[i32, 3]): i32") !=
              std::string::npos);
  EXPECT_TRUE(koopa.find("call @sum(") != std::string::npos);
}
