#include "compiler/ir/koopa_generator.h"
#include "compiler/lexer/token_rules.h"
#include "compiler/parser/grammar_rules.h"
#include "compiler/riscv/riscv_generator.h"

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
    throw CliError(
        "usage: compiler (-koopa|-riscv|-perf) <input-file> -o <output-file>");
  }
  if (std::string(argv[3]) != "-o") {
    throw CliError("missing -o before output file");
  }

  return {argv[1], argv[2], argv[4]};
}

std::unique_ptr<compiler::parser::ParseNode>
parseSource(const std::string &input_path) {
  std::ifstream input(input_path);
  if (!input) {
    throw InputFileError("cannot open input file: " + input_path);
  }

  compiler::lexer::Lexer lexer = compiler::lexer::buildDefaultLexer();
  compiler::parser::Parser parser = compiler::parser::buildDefaultParser();
  return parser.parse(lexer.tokenize(input));
}

void compileToKoopa(const CompilerOptions &options) {
  std::unique_ptr<compiler::parser::ParseNode> ast =
      parseSource(options.input_path);

  std::ofstream output(options.output_path);
  if (!output) {
    throw OutputFileError("cannot open output file: " + options.output_path);
  }

  compiler::ir::KoopaGenerator generator;
  generator.generate(*ast, output);
}

void compileToRiscv(const CompilerOptions &options) {
  std::unique_ptr<compiler::parser::ParseNode> ast =
      parseSource(options.input_path);

  std::ofstream output(options.output_path);
  if (!output) {
    throw OutputFileError("cannot open output file: " + options.output_path);
  }

  compiler::ir::KoopaGenerator koopa_generator;
  compiler::riscv::RiscvGenerator riscv_generator;

  riscv_generator.generate(koopa_generator.generate(*ast), output);
}

void compileToOptimizedRiscv(const CompilerOptions &options) {
  std::unique_ptr<compiler::parser::ParseNode> ast =
      parseSource(options.input_path);

  std::ofstream output(options.output_path);
  if (!output) {
    throw OutputFileError("cannot open output file: " + options.output_path);
  }

  compiler::ir::KoopaGenerator koopa_generator;
  compiler::riscv::RiscvGenerator riscv_generator;

  riscv_generator.generateOptimized(koopa_generator.generate(*ast), output);
}

bool contains(const std::string &text, const std::string &pattern) {
  return text.find(pattern) != std::string::npos;
}

int parserExitCode(const compiler::parser::ParserError &error) {
  const std::string message = error.what();

  // 20: input had valid prefixes, but parser stopped before consuming all tokens.
  if (contains(message, "unexpected trailing token")) {
    return 20;
  }

  // 21: top-level, function-definition, and function-parameter syntax.
  if (contains(message, "CompUnit") || contains(message, "TopItem") ||
      contains(message, "FuncDef") || contains(message, "FuncType") ||
      contains(message, "FuncFParam")) {
    return 21;
  }

  // 22: declarations and initializer lists.
  if (contains(message, "Decl") || contains(message, "BType") ||
      contains(message, "Const") || contains(message, "VarDef") ||
      contains(message, "InitVal")) {
    return 22;
  }

  // 23: block and statement syntax.
  if (contains(message, "Block") || contains(message, "Stmt") ||
      contains(message, "ElseOpt") || contains(message, "ReturnExpOpt")) {
    return 23;
  }

  // 24: lvalues, primary expressions, unary expressions, and function calls.
  if (contains(message, "LVal") || contains(message, "PrimaryExp") ||
      contains(message, "UnaryExp") || contains(message, "UnaryOp") ||
      contains(message, "FuncRParams")) {
    return 24;
  }

  // 25: arithmetic expression layers.
  if (contains(message, "MulExp") || contains(message, "AddExp")) {
    return 25;
  }

  // 26: comparison and logical expression layers.
  if (contains(message, "RelExp") || contains(message, "EqExp") ||
      contains(message, "LAndExp") || contains(message, "LOrExp") ||
      contains(message, "Exp")) {
    return 26;
  }

  // 27: direct terminal mismatch, e.g. "expected SEMICOLON".
  if (contains(message, "expected ")) {
    return 27;
  }

  return 11;
}

} // namespace

int main(int argc, const char *argv[]) {
  try {
    CompilerOptions options = parseOptions(argc, argv);
    if (options.mode == "-koopa") {
      compileToKoopa(options);
    } else if (options.mode == "-riscv") {
      compileToRiscv(options);
    } else if (options.mode == "-perf") {
      compileToOptimizedRiscv(options);
    } else {
      throw CliError("unsupported output mode: " + options.mode);
    }
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
    return parserExitCode(error);
  } catch (const compiler::ir::IrError &error) {
    std::cerr << "[ir] " << error.what() << '\n';
    return 12;
  } catch (const compiler::riscv::RiscvError &error) {
    std::cerr << "[riscv] " << error.what() << '\n';
    return 13;
  } catch (const std::exception &error) {
    std::cerr << "[internal] " << error.what() << '\n';
    return 1;
  }

  return 0;
}
