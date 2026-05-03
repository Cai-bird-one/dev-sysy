#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

struct CompilerOptions {
  std::string mode;
  std::string input_path;
  std::string output_path;
};

class CliError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class InputFileError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class OutputFileError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

CompilerOptions parseOptions(int argc, const char *argv[]) {
  if (argc != 5) {
    throw CliError("usage: compiler -koopa <input-file> -o <output-file>");
  }
  if (std::string(argv[3]) != "-o") {
    throw CliError("missing -o before output file");
  }

  return {argv[1], argv[2], argv[4]};
}

void compileToKoopa(const CompilerOptions &options) {
  std::ifstream input(options.input_path);
  if (!input) {
    throw InputFileError("cannot open input file: " + options.input_path);
  }

  std::ofstream output(options.output_path);
  if (!output) {
    throw OutputFileError("cannot open output file: " + options.output_path);
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
      throw CliError("unsupported output mode: " + options.mode);
    }
    compileToKoopa(options);
  } catch (const CliError &error) {
    std::cerr << "[cli] " << error.what() << '\n';
    return 2;
  } catch (const InputFileError &error) {
    std::cerr << "[input] " << error.what() << '\n';
    return 3;
  } catch (const OutputFileError &error) {
    std::cerr << "[output] " << error.what() << '\n';
    return 4;
  } catch (const compiler::lexer::LexerError &error) {
    std::cerr << "[lexer] " << error.what() << '\n';
    return 10;
  } catch (const compiler::parser::ParserError &error) {
    std::cerr << "[parser] " << error.what() << '\n';
    return 11;
  } catch (const compiler::ir::IrError &error) {
    std::cerr << "[ir] " << error.what() << '\n';
    return 12;
  } catch (const std::exception &error) {
    std::cerr << "[internal] " << error.what() << '\n';
    return 1;
  }

  return 0;
}
