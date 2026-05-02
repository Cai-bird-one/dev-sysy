#include "compiler/lexer/token_rules.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: dump_tokens <source-file>\n";
    return 1;
  }

  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "cannot open source file: " << argv[1] << '\n';
    return 1;
  }

  try {
    compiler::lexer::Lexer lexer = compiler::lexer::buildDefaultLexer();
    for (const compiler::lexer::Token &token : lexer.tokenize(input)) {
      std::cout << token.name << '\t' << token.lexeme << '\n';
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
