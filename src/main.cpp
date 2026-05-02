#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

struct CompilerOptions {
  std::string mode;
  std::string input_path;
  std::string output_path;
};

CompilerOptions parseOptions(int argc, const char *argv[]) {
  if (argc != 5) {
    throw std::runtime_error(
        "usage: compiler -koopa <input-file> -o <output-file>");
  }
  if (std::string(argv[3]) != "-o") {
    throw std::runtime_error("missing -o before output file");
  }

  return {argv[1], argv[2], argv[4]};
}

void compileToKoopa(const CompilerOptions &options) {
  std::ifstream input(options.input_path);
  if (!input) {
    throw std::runtime_error("cannot open input file: " + options.input_path);
  }

  std::ofstream output(options.output_path);
  if (!output) {
    throw std::runtime_error("cannot open output file: " + options.output_path);
  }

  compiler::lexer::Lexer lexer = compiler::lexer::buildDefaultLexer();
  compiler::parser::Parser parser = compiler::parser::buildDefaultParser();
  compiler::ir::KoopaGenerator generator;

  std::unique_ptr<compiler::parser::ParseNode> ast =
      parser.parse(lexer.tokenize(input));
  generator.generate(*ast, output);
}

} // namespace

int main(int argc, const char *argv[]) {
  try {
    CompilerOptions options = parseOptions(argc, argv);
    if (options.mode != "-koopa") {
      throw std::runtime_error("unsupported output mode: " + options.mode);
    }
    compileToKoopa(options);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
