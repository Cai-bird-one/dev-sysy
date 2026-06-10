#pragma once

#include "compiler/ir/emit/declaration/declaration_translator.h"
#include "compiler/ir/emit/program/program_context.h"
#include "compiler/ir/model/ir_model.h"
#include "compiler/parser/parser.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::ir {

class ProgramBuilder : public DeclarationContext {
public:
  std::string generate(const compiler::parser::ParseNode &ast);

private:
  void collectGlobalDeclarations(const compiler::parser::ParseNode &node);
  void collectDeclaration(const compiler::parser::ParseNode &node);
  void emitConstDefinition(const compiler::parser::ParseNode &node) override;
  void emitVarDefinition(const compiler::parser::ParseNode &node) override;
  Value emitGlobalExpression(const compiler::parser::ParseNode &node);
  std::vector<long long>
  collectGlobalArrayDimensions(const compiler::parser::ParseNode &node);
  void collectInitializerChildren(
      const compiler::parser::ParseNode &node,
      std::vector<const compiler::parser::ParseNode *> &out);
  bool isInitializerList(const compiler::parser::ParseNode &node) const;
  long long fillGlobalInitializer(const compiler::parser::ParseNode &node,
                                  const std::vector<long long> &dimensions,
                                  size_t depth, long long begin,
                                  std::vector<long long> &values);
  std::string formatArrayInitializer(const std::vector<long long> &values,
                                     const std::vector<long long> &dimensions,
                                     size_t depth, long long begin);
  std::string newGlobalValue(const std::string &name);
  void defineGlobal(const std::string &name, Symbol symbol);
  const Symbol &lookupGlobal(const std::string &name) const;

  ProgramContext context_;
};

} // namespace compiler::ir
