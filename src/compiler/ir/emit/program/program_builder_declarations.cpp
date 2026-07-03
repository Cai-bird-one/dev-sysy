#include "compiler/ir/emit/program/program_builder.h"

#include "compiler/ir/emit/declaration/declaration_translator.h"
#include "compiler/ir/emit/expression/constant_expression.h"
#include "compiler/ir/tensor/koopa_tensor_ir.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

namespace compiler::ir {

void ProgramBuilder::collectDeclaration(
    const compiler::parser::ParseNode &node) {
  DeclarationTranslator translator(*this);
  translator.translate(node);
}

void ProgramBuilder::emitConstDefinition(
    const compiler::parser::ParseNode &node, SourceValueType type) {
  if (node.children.size() != 4 || node.children[0]->symbol != "IDENT") {
    throw IrError("invalid ConstDef node");
  }
  std::vector<long long> dimensions =
      collectGlobalArrayDimensions(*node.children[1]);
  if (type == SourceValueType::Tensor && dimensions.empty()) {
    throw IrError("tensor declaration requires at least one dimension: " +
                  node.children[0]->lexeme);
  }
  if (!dimensions.empty()) {
    std::string pointer = newGlobalValue(node.children[0]->lexeme);
    std::vector<long long> values(elementCount(dimensions), 0);
    fillGlobalInitializer(*node.children[3], dimensions, 0, 0, values);
    if (type == SourceValueType::Tensor) {
      context_.addGlobalInstruction(
          formatTensorShapeDecl(pointer, tensorShapeFromDimensions(dimensions)));
    }
    context_.addGlobalInstruction("global " + pointer + " = alloc " +
                                  arrayType(dimensions) + ", " +
                                  formatArrayInitializer(values, dimensions, 0,
                                                         0));
    defineGlobal(node.children[0]->lexeme,
                 Symbol{SymbolKind::Variable, 0, pointer, dimensions, false,
                        false, type});
    return;
  }
  long long value =
      expectConstant(emitGlobalExpression(*node.children[3]), "const initializer");
  defineGlobal(node.children[0]->lexeme,
               Symbol{SymbolKind::Constant, value, "", {}, false, false});
}

void ProgramBuilder::emitVarDefinition(
    const compiler::parser::ParseNode &node, SourceValueType type) {
  if (node.children.size() < 2 || node.children[0]->symbol != "IDENT") {
    throw IrError("invalid VarDef node");
  }
  long long value = 0;
  bool has_initializer = false;
  std::vector<long long> dimensions =
      collectGlobalArrayDimensions(*node.children[1]);
  if (type == SourceValueType::Tensor && dimensions.empty()) {
    throw IrError("tensor declaration requires at least one dimension: " +
                  node.children[0]->lexeme);
  }
  if (!dimensions.empty()) {
    std::string pointer = newGlobalValue(node.children[0]->lexeme);
    std::vector<long long> values(elementCount(dimensions), 0);
    if (type == SourceValueType::Tensor) {
      context_.addGlobalInstruction(
          formatTensorShapeDecl(pointer, tensorShapeFromDimensions(dimensions)));
    }
    if (node.children.size() >= 3 && !node.children[2]->children.empty()) {
      fillGlobalInitializer(*node.children[2]->children[1], dimensions, 0, 0,
                            values);
      context_.addGlobalInstruction("global " + pointer + " = alloc " +
                                    arrayType(dimensions) + ", " +
                                    formatArrayInitializer(values, dimensions,
                                                           0, 0));
    } else {
      context_.addGlobalInstruction("global " + pointer + " = alloc " +
                                    arrayType(dimensions) + ", zeroinit");
    }
    defineGlobal(node.children[0]->lexeme,
                 Symbol{SymbolKind::Variable, 0, pointer, dimensions, true,
                        false, type});
    return;
  }
  if (node.children.size() >= 3 && !node.children[2]->children.empty()) {
    has_initializer = true;
    value = expectConstant(emitGlobalExpression(*node.children[2]->children[1]),
                           "global variable initializer");
  }
  std::string pointer = newGlobalValue(node.children[0]->lexeme);
  context_.addGlobalInstruction("global " + pointer + " = alloc i32, " +
                                (has_initializer ? std::to_string(value)
                                                 : "zeroinit"));
  defineGlobal(node.children[0]->lexeme,
               Symbol{SymbolKind::Variable, 0, pointer, {}, true, false, type});
}

Value ProgramBuilder::emitGlobalExpression(
    const compiler::parser::ParseNode &node) {
  ConstantExpressionEvaluator evaluator(
      [this](const std::string &name) -> const Symbol & {
        return lookupGlobal(name);
      });
  return evaluator.evaluate(node);
}

} // namespace compiler::ir
