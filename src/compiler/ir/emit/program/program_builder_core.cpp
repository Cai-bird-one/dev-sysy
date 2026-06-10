#include "compiler/ir/emit/program/program_builder.h"

#include "compiler/ir/ast/parse_node_utils.h"
#include "compiler/ir/emit/function/function_builder.h"
#include "compiler/ir/analysis/semantic_collector.h"
#include "compiler/ir/koopa_generator.h"

#include <sstream>
#include <utility>

namespace compiler::ir {

std::string ProgramBuilder::generate(const compiler::parser::ParseNode &ast) {
  collectGlobalDeclarations(ast);
  SemanticProgram semantic =
      FunctionSemanticCollector(context_.globalSymbols()).analyze(ast);
  context_.setFunctionSignatures(std::move(semantic.function_signatures));
  context_.addReservedValues(semantic.reserved_values);

  std::vector<const compiler::parser::ParseNode *> functions;
  collectNodes(ast, "FuncDef", functions);
  if (functions.empty()) {
    throw IrError("cannot find function definition in AST");
  }

  std::vector<std::string> function_outputs;
  for (const compiler::parser::ParseNode *function : functions) {
    FunctionBuilder builder(*function, context_.globalSymbols(),
                            context_.functionSignatures(),
                            context_.usedValues(),
                            context_.usedExternalFunctions());
    function_outputs.push_back(builder.generate());
  }

  std::ostringstream output;
  for (const std::string &line : context_.globalInstructions()) {
    output << line << '\n';
  }
  for (const std::string &name : context_.usedExternalFunctions()) {
    const FunctionSignature &signature = context_.functionSignature(name);
    output << "decl @" << name << "(";
    for (size_t i = 0; i < signature.parameter_count; ++i) {
      if (i != 0) {
        output << ", ";
      }
      output << (i < signature.parameter_types.size()
                     ? signature.parameter_types[i]
                     : std::string("i32"));
    }
    output << ")";
    if (signature.return_type != "void") {
      output << ": " << signature.return_type;
    }
    output << '\n';
  }
  if (!context_.globalInstructions().empty() ||
      !context_.usedExternalFunctions().empty()) {
    output << '\n';
  }
  for (size_t i = 0; i < function_outputs.size(); ++i) {
    output << function_outputs[i];
    if (i + 1 != function_outputs.size()) {
      output << '\n';
    }
  }
  return output.str();
}

void ProgramBuilder::collectGlobalDeclarations(
    const compiler::parser::ParseNode &node) {
  if (node.symbol == "Block") {
    return;
  }
  if (node.symbol == "TopItem") {
    if (findDirectChild(node, "FuncDef") != nullptr) {
      return;
    }
    if (findDirectChild(node, "Decl") != nullptr) {
      collectDeclaration(node);
    }
    return;
  }
  for (const auto &child : node.children) {
    collectGlobalDeclarations(*child);
  }
}

std::string ProgramBuilder::newGlobalValue(const std::string &name) {
  return context_.newGlobalValue(name);
}

void ProgramBuilder::defineGlobal(const std::string &name, Symbol symbol) {
  context_.defineGlobal(name, std::move(symbol));
}

const Symbol &ProgramBuilder::lookupGlobal(const std::string &name) const {
  return context_.lookupGlobal(name);
}

} // namespace compiler::ir
