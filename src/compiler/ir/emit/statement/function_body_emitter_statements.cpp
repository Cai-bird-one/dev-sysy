#include "compiler/ir/emit/function/function_body_emitter.h"

#include "compiler/ir/koopa_generator.h"

#include <utility>

namespace compiler::ir {

bool FunctionBodyEmitter::emitBlock(const compiler::parser::ParseNode &node,
                                bool create_scope) {
  if (node.symbol != "Block") {
    throw IrError("expected Block node");
  }
  if (create_scope) {
    pushScope();
  }
  bool did_return = false;
  for (const auto &child : node.children) {
    if (child->symbol == "BlockItems") {
      did_return = emitBlockItems(*child);
      break;
    }
    if (child->symbol == "BlockItem") {
      if (emitBlockItem(*child)) {
        did_return = true;
        break;
      }
    }
    if (child->symbol == "Stmt" && emitStatement(*child)) {
      did_return = true;
      break;
    }
    if (child->symbol == "Decl") {
      collectDeclaration(*child);
    }
  }
  if (create_scope) {
    popScope();
  }
  return did_return;
}

bool FunctionBodyEmitter::emitBlockItems(const compiler::parser::ParseNode &node) {
  if (node.children.empty()) {
    return false;
  }
  for (const auto &child : node.children) {
    if (child->symbol == "BlockItem") {
      if (emitBlockItem(*child)) {
        return true;
      }
    } else if (child->symbol == "BlockItems") {
      return emitBlockItems(*child);
    }
  }
  return false;
}

bool FunctionBodyEmitter::emitBlockItem(const compiler::parser::ParseNode &node) {
  for (const auto &child : node.children) {
    if (child->symbol == "Decl") {
      collectDeclaration(*child);
      return false;
    }
    if (child->symbol == "Stmt") {
      return emitStatement(*child);
    }
  }
  throw IrError("invalid BlockItem node");
}

bool FunctionBodyEmitter::emitStatement(const compiler::parser::ParseNode &node) {
  StatementTranslator translator(*this);
  return translator.translate(node);
}

void FunctionBodyEmitter::pushScope() { context_.pushScope(); }

void FunctionBodyEmitter::popScope() { context_.popScope(); }

void FunctionBodyEmitter::emitInstruction(std::string instruction) {
  emit(std::move(instruction));
}

const std::string &FunctionBodyEmitter::returnType() const {
  return context_.returnType();
}

const std::string &FunctionBodyEmitter::functionName() const {
  return context_.functionName();
}

void FunctionBodyEmitter::markReturned() { context_.markReturned(); }

bool FunctionBodyEmitter::hasLoop() const { return context_.hasLoop(); }

StatementLoopLabels FunctionBodyEmitter::currentLoop() const {
  return context_.currentLoop();
}

void FunctionBodyEmitter::pushLoop(StatementLoopLabels labels) {
  context_.pushLoop(std::move(labels));
}

void FunctionBodyEmitter::popLoop() { context_.popLoop(); }

} // namespace compiler::ir
