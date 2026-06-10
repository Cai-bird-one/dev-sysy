#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/emit/declaration/declaration_translator.h"
#include "compiler/ir/koopa_generator.h"
#include "compiler/ir/util/ir_utils.h"

#include <utility>

namespace compiler::ir {

void FunctionBodyEmitter::collectDeclaration(
    const compiler::parser::ParseNode &node) {
  DeclarationTranslator translator(*this);
  translator.translate(node);
}

void FunctionBodyEmitter::emitConstDefinition(
    const compiler::parser::ParseNode &node) {
  if (node.children.size() != 4 || node.children[0]->symbol != "IDENT") {
    throw IrError("invalid ConstDef node");
  }
  std::vector<long long> dimensions = collectArrayDimensions(*node.children[1]);
  if (!dimensions.empty()) {
    collectLocalArrayDef(node.children[0]->lexeme, dimensions,
                         node.children[3].get(), false);
    return;
  }
  long long value =
      expectConstant(emitExpression(*node.children[3]), "const initializer");
  define(node.children[0]->lexeme,
         Symbol{SymbolKind::Constant, value, "", {}, false, false});
}

void FunctionBodyEmitter::emitVarDefinition(
    const compiler::parser::ParseNode &node) {
  if (node.children.size() < 2 || node.children[0]->symbol != "IDENT") {
    throw IrError("invalid VarDef node");
  }
  const std::string &name = node.children[0]->lexeme;
  std::vector<long long> dimensions = collectArrayDimensions(*node.children[1]);
  if (!dimensions.empty()) {
    const compiler::parser::ParseNode *initializer = nullptr;
    if (node.children.size() >= 3 && !node.children[2]->children.empty()) {
      initializer = node.children[2]->children[1].get();
    }
    collectLocalArrayDef(name, dimensions, initializer, true);
    return;
  }

  std::string pointer = newNamedValue(name);
  emitLocalAlloc(pointer + " = alloc i32");

  Value initial_value;
  if (node.children.size() >= 3 && !node.children[2]->children.empty()) {
    initial_value = emitExpression(*node.children[2]->children[1]);
  } else {
    initial_value = makeConstant(0);
  }
  define(name, Symbol{SymbolKind::Variable, 0, pointer, {}, true, false});
  emit("store " + initial_value.operand + ", " + pointer);
}

void FunctionBodyEmitter::collectLocalArrayDef(
    const std::string &name, const std::vector<long long> &dimensions,
    const compiler::parser::ParseNode *initializer, bool assignable) {
  std::string pointer = newNamedValue(name);
  emitLocalAlloc(pointer + " = alloc " + arrayType(dimensions));
  define(name, Symbol{SymbolKind::Variable, 0, pointer, dimensions, assignable,
                      false});

  long long count = elementCount(dimensions);
  for (long long i = 0; i < count; ++i) {
    std::string element_pointer = emitArrayElementPointer(pointer, dimensions, i);
    emit("store 0, " + element_pointer);
  }
  if (initializer == nullptr) {
    return;
  }

  std::vector<std::pair<long long, Value>> entries;
  flattenRuntimeInitializer(*initializer, dimensions, 0, 0, entries);
  for (const auto &entry : entries) {
    if (entry.first < 0 || entry.first >= count) {
      throw IrError("too many array initializer values for: " + name);
    }
    std::string element_pointer =
        emitArrayElementPointer(pointer, dimensions, entry.first);
    emit("store " + entry.second.operand + ", " + element_pointer);
  }
}

} // namespace compiler::ir
