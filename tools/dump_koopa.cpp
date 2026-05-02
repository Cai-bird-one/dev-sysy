#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"

#include <exception>
#include <fstream>
#include <iostream>

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: dump_koopa <source-file>\n";
    return 1;
  }

  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "cannot open source file: " << argv[1] << '\n';
    return 1;
  }

  try {
    compiler::lexer::Lexer lexer = compiler::lexer::buildDefaultLexer();
    compiler::parser::Parser parser = compiler::parser::buildDefaultParser();
    compiler::ir::KoopaGenerator generator;
    std::unique_ptr<compiler::parser::ParseNode> ast =
        parser.parse(lexer.tokenize(input));
    generator.generate(*ast, std::cout);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
