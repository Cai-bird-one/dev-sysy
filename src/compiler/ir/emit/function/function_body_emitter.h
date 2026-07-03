#pragma once

#include "compiler/ir/emit/declaration/declaration_translator.h"
#include "compiler/ir/emit/expression/runtime_expression_translator.h"
#include "compiler/ir/emit/function/function_context.h"
#include "compiler/ir/emit/statement/statement_translator.h"
#include "compiler/ir/model/ir_model.h"
#include "compiler/parser/parser.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::ir {

class FunctionBodyEmitter : public RuntimeExpressionContext,
                            public DeclarationContext,
                            public StatementEmitContext {
public:
  FunctionBodyEmitter(std::map<std::string, Symbol> global_symbols,
                      std::map<std::string, FunctionSignature>
                          function_signatures,
                      std::set<std::string> reserved_values,
                      std::set<std::string> &used_external_functions,
                      std::string function_name, std::string return_type);

  bool emitBlock(const compiler::parser::ParseNode &node,
                 bool create_scope) override;
  void collectDeclaration(const compiler::parser::ParseNode &node);
  std::vector<long long>
  collectArrayDimensions(const compiler::parser::ParseNode &node);
  std::string newNamedValue(const std::string &name);
  std::string newParameterValue(const std::string &name);
  void define(const std::string &name, Symbol symbol);
  void emitLocalAlloc(std::string instruction);
  Value emitExpression(const compiler::parser::ParseNode &node) override;
  void emitInstruction(std::string instruction) override;
  bool blockTerminated() const;
  const std::vector<std::string> &entryAllocs() const;
  const std::vector<std::string> &instructions() const;

private:
  bool emitBlockItems(const compiler::parser::ParseNode &node);
  bool emitBlockItem(const compiler::parser::ParseNode &node);
  bool emitStatement(const compiler::parser::ParseNode &node);
  void pushScope();
  void popScope();

  void emitConstDefinition(const compiler::parser::ParseNode &node,
                           SourceValueType type) override;
  void emitVarDefinition(const compiler::parser::ParseNode &node,
                         SourceValueType type) override;
  void collectLocalArrayDef(const std::string &name,
                            const std::vector<long long> &dimensions,
                            const compiler::parser::ParseNode *initializer,
                            bool assignable, SourceValueType type);

  Value emitCall(const compiler::parser::ParseNode &node) override;
  void collectCallArgumentNodes(
      const compiler::parser::ParseNode &node,
      std::vector<const compiler::parser::ParseNode *> &args);
  const compiler::parser::ParseNode *
  unwrapArrayArgument(const compiler::parser::ParseNode &node);
  Value emitPointerArgument(const compiler::parser::ParseNode &node,
                            const std::string &parameter_type);
  Value emitShortCircuitTail(const compiler::parser::ParseNode &node, Value lhs,
                             bool is_or) override;
  Value emitBinary(const std::string &op, Value lhs, Value rhs) override;
  Value emitBoolean(Value value) override;
  Value emitLVal(const compiler::parser::ParseNode &node) override;

  std::string
  lookupVariablePointer(const compiler::parser::ParseNode &lval) override;
  std::string emitArrayAccessPointer(const Symbol &symbol,
                                     const std::vector<Value> &indices);
  std::string emitDecayedArrayPointer(
      const Symbol &symbol, const std::vector<Value> &indices,
      const std::vector<long long> &expected_dimensions);
  std::vector<Value> collectLValIndices(const compiler::parser::ParseNode &lval);
  std::string emitArrayElementPointer(const std::string &base_pointer,
                                      const std::vector<long long> &dimensions,
                                      long long linear_index);
  std::string emitArrayElementPointer(const std::string &base_pointer,
                                      const std::vector<long long> &dimensions,
                                      const std::vector<Value> &indices,
                                      bool first_getptr);

  void collectInitializerChildren(
      const compiler::parser::ParseNode &node,
      std::vector<const compiler::parser::ParseNode *> &out);
  bool isInitializerList(const compiler::parser::ParseNode &node) const;
  long long flattenRuntimeInitializer(
      const compiler::parser::ParseNode &node,
      const std::vector<long long> &dimensions, size_t depth, long long begin,
      std::vector<std::pair<long long, Value>> &entries);

  Value makeConstant(long long value) const override;
  void emit(std::string instruction);
  void emitLabel(const std::string &label) override;
  const std::string &returnType() const override;
  const std::string &functionName() const override;
  void markReturned() override;
  bool hasLoop() const override;
  StatementLoopLabels currentLoop() const override;
  void pushLoop(StatementLoopLabels labels) override;
  void popLoop() override;
  std::string newTemp();
  std::string newLabel(const std::string &prefix) override;
  const Symbol &lookup(const std::string &name) const;

  FunctionContext context_;
};

} // namespace compiler::ir
