#include "compiler/ir/koopa_generator.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <numeric>
#include <ostream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace compiler::ir {
namespace {

struct Value {
  bool constant = false;
  long long const_value = 0;
  std::string operand;
};

enum class SymbolKind {
  Constant,
  Variable,
};

struct Symbol {
  SymbolKind kind = SymbolKind::Constant;
  long long const_value = 0;
  std::string pointer;
  std::vector<long long> dimensions;
  bool assignable = false;
  bool pointer_parameter = false;
};

struct FunctionSignature {
  std::string return_type = "i32";
  size_t parameter_count = 0;
  bool external = false;
  std::vector<std::string> parameter_types;
};

const compiler::parser::ParseNode *
findFirst(const compiler::parser::ParseNode &node, const std::string &symbol) {
  if (node.symbol == symbol) {
    return &node;
  }
  for (const auto &child : node.children) {
    const compiler::parser::ParseNode *found = findFirst(*child, symbol);
    if (found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

const compiler::parser::ParseNode *
findDirectChild(const compiler::parser::ParseNode &node,
                const std::string &symbol) {
  for (const auto &child : node.children) {
    if (child->symbol == symbol) {
      return child.get();
    }
  }
  return nullptr;
}

void collectNodes(const compiler::parser::ParseNode &node,
                  const std::string &symbol,
                  std::vector<const compiler::parser::ParseNode *> &out) {
  if (node.symbol == symbol) {
    out.push_back(&node);
  }
  for (const auto &child : node.children) {
    collectNodes(*child, symbol, out);
  }
}

bool hasNonEmptyChild(const compiler::parser::ParseNode &node,
                      const std::string &symbol) {
  const compiler::parser::ParseNode *child = findDirectChild(node, symbol);
  return child != nullptr && !child->children.empty();
}

std::string toOperand(long long value) { return std::to_string(value); }

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

size_t findTopLevelComma(const std::string &text, size_t begin = 0) {
  int depth = 0;
  for (size_t i = begin; i < text.size(); ++i) {
    if (text[i] == '[') {
      ++depth;
    } else if (text[i] == ']') {
      --depth;
    } else if (text[i] == ',' && depth == 0) {
      return i;
    }
  }
  return std::string::npos;
}

std::string trim(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string koopaOp(const std::string &token) {
  if (token == "PLUS") {
    return "add";
  }
  if (token == "MINUS") {
    return "sub";
  }
  if (token == "STAR") {
    return "mul";
  }
  if (token == "SLASH") {
    return "div";
  }
  if (token == "PERCENT") {
    return "mod";
  }
  if (token == "LT") {
    return "lt";
  }
  if (token == "GT") {
    return "gt";
  }
  if (token == "LE") {
    return "le";
  }
  if (token == "GE") {
    return "ge";
  }
  if (token == "EQ") {
    return "eq";
  }
  if (token == "NE") {
    return "ne";
  }
  if (token == "AND") {
    return "and";
  }
  if (token == "OR") {
    return "or";
  }
  throw IrError("unsupported binary operator: " + token);
}

long long foldBinary(const std::string &op, long long lhs, long long rhs) {
  if (op == "PLUS") {
    return lhs + rhs;
  }
  if (op == "MINUS") {
    return lhs - rhs;
  }
  if (op == "STAR") {
    return lhs * rhs;
  }
  if (op == "SLASH") {
    return lhs / rhs;
  }
  if (op == "PERCENT") {
    return lhs % rhs;
  }
  if (op == "LT") {
    return lhs < rhs;
  }
  if (op == "GT") {
    return lhs > rhs;
  }
  if (op == "LE") {
    return lhs <= rhs;
  }
  if (op == "GE") {
    return lhs >= rhs;
  }
  if (op == "EQ") {
    return lhs == rhs;
  }
  if (op == "NE") {
    return lhs != rhs;
  }
  if (op == "AND") {
    return (lhs != 0) && (rhs != 0);
  }
  if (op == "OR") {
    return (lhs != 0) || (rhs != 0);
  }
  throw IrError("unsupported binary operator: " + op);
}

long long expectConstant(const Value &value, const std::string &context) {
  if (!value.constant) {
    throw IrError(context + " must be a constant expression");
  }
  return value.const_value;
}

long long elementCount(const std::vector<long long> &dimensions,
                       size_t begin = 0) {
  long long count = 1;
  for (size_t i = begin; i < dimensions.size(); ++i) {
    count *= dimensions[i];
  }
  return count;
}

std::string arrayType(const std::vector<long long> &dimensions,
                      size_t begin = 0) {
  if (begin == dimensions.size()) {
    return "i32";
  }
  return "[" + arrayType(dimensions, begin + 1) + ", " +
         std::to_string(dimensions[begin]) + "]";
}

std::vector<long long> parseArrayTypeDimensions(const std::string &type) {
  std::string text = trim(type);
  if (text == "i32") {
    return {};
  }
  if (text.empty() || text.front() != '[' || text.back() != ']') {
    throw IrError("invalid array type: " + type);
  }
  std::string inner = text.substr(1, text.size() - 2);
  size_t comma = findTopLevelComma(inner);
  if (comma == std::string::npos) {
    throw IrError("invalid array type: " + type);
  }
  std::vector<long long> dimensions;
  dimensions.push_back(std::stoll(trim(inner.substr(comma + 1))));
  std::vector<long long> tail =
      parseArrayTypeDimensions(inner.substr(0, comma));
  dimensions.insert(dimensions.end(), tail.begin(), tail.end());
  return dimensions;
}

std::vector<long long> parsePointerTypeDimensions(const std::string &type) {
  if (!startsWith(type, "*")) {
    throw IrError("expected pointer type: " + type);
  }
  return parseArrayTypeDimensions(type.substr(1));
}

std::string findFunctionName(const compiler::parser::ParseNode &function);
std::string findFunctionReturnType(const compiler::parser::ParseNode &function);

class FunctionBuilder {
public:
  FunctionBuilder(const compiler::parser::ParseNode &function,
                  std::map<std::string, Symbol> global_symbols,
                  std::map<std::string, FunctionSignature> function_signatures,
                  std::set<std::string> reserved_values,
                  std::set<std::string> &used_external_functions)
      : function_(function), function_signatures_(std::move(function_signatures)),
        used_external_functions_(used_external_functions),
        used_values_(std::move(reserved_values)) {
    function_name_ = findFunctionName(function_);
    return_type_ = findFunctionReturnType(function_);
    scopes_.push_back(std::move(global_symbols));
    scopes_.push_back({});
  }

  std::string generate() {
    collectParameters();
    const compiler::parser::ParseNode *block = findDirectChild(function_, "Block");
    if (block == nullptr) {
      block = findFirst(function_, "Block");
    }
    if (block == nullptr) {
      throw IrError("cannot find function block in AST");
    }

    bool block_returned = emitBlock(*block, false);
    if (!block_returned && !block_terminated_) {
      emit(return_type_ == "void" ? std::string("ret") : std::string("ret 0"));
    }

    std::ostringstream output;
    output << "fun @" << function_name_ << "(";
    for (size_t i = 0; i < parameters_.size(); ++i) {
      if (i != 0) {
        output << ", ";
      }
      output << parameters_[i].koopa_name << ": " << parameters_[i].type;
    }
    output << ")";
    if (return_type_ != "void") {
      output << ": " << return_type_;
    }
    output << " {\n%entry:\n";
    for (const std::string &line : entry_allocs_) {
      output << "  " << line << '\n';
    }
    for (const Parameter &parameter : parameters_) {
      if (parameter.pointer.empty()) {
        continue;
      }
      output << "  store " << parameter.koopa_name << ", " << parameter.pointer
             << '\n';
    }
    for (const std::string &line : instructions_) {
      if (!line.empty() && line.back() == ':') {
        output << line << '\n';
      } else {
        output << "  " << line << '\n';
      }
    }
    output << "}\n";
    return output.str();
  }

private:
  bool emitBlock(const compiler::parser::ParseNode &node, bool create_scope) {
    if (node.symbol != "Block") {
      throw IrError("expected Block node");
    }
    if (create_scope) {
      scopes_.push_back({});
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
      if (child->symbol == "Stmt") {
        if (emitStmt(*child)) {
          did_return = true;
          break;
        }
      }
      if (child->symbol == "Decl") {
        collectDeclaration(*child, false);
      }
    }
    if (create_scope) {
      scopes_.pop_back();
    }
    return did_return;
  }

  bool emitBlockItems(const compiler::parser::ParseNode &node) {
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

  bool emitBlockItem(const compiler::parser::ParseNode &node) {
    for (const auto &child : node.children) {
      if (child->symbol == "Decl") {
        collectDeclaration(*child, false);
        return false;
      }
      if (child->symbol == "Stmt") {
        return emitStmt(*child);
      }
    }
    throw IrError("invalid BlockItem node");
  }

  bool emitStmt(const compiler::parser::ParseNode &node) {
    if (node.children.empty()) {
      throw IrError("invalid Stmt node");
    }

    if (node.children[0]->symbol == "RETURN") {
      const compiler::parser::ParseNode *return_exp =
          findDirectChild(node, "ReturnExpOpt");
      if (return_exp != nullptr && !return_exp->children.empty()) {
        if (return_type_ == "void") {
          throw IrError("void function cannot return a value: " + function_name_);
        }
        emit("ret " + emitExpression(*return_exp).operand);
      } else {
        const compiler::parser::ParseNode *old_exp = findDirectChild(node, "Exp");
        if (old_exp != nullptr) {
          if (return_type_ == "void") {
            throw IrError("void function cannot return a value: " + function_name_);
          }
          emit("ret " + emitExpression(*old_exp).operand);
        } else if (return_type_ == "void") {
          emit("ret");
        } else {
          emit("ret 0");
        }
      }
      returned_ = true;
      return true;
    }

    if (node.children[0]->symbol == "LVal") {
      if (node.children.size() >= 3 && node.children[1]->symbol == "ASSIGN") {
        std::string pointer = lookupVariablePointer(*node.children[0]);
        Value value = emitExpression(*node.children[2]);
        emit("store " + value.operand + ", " + pointer);
        return false;
      }
    }

    if (node.children[0]->symbol == "IF") {
      return emitIfStmt(node);
    }

    if (node.children[0]->symbol == "WHILE") {
      return emitWhileStmt(node);
    }

    if (node.children[0]->symbol == "BREAK") {
      if (loop_stack_.empty()) {
        throw IrError("break statement outside loop");
      }
      emit("jump " + loop_stack_.back().break_label);
      return true;
    }

    if (node.children[0]->symbol == "CONTINUE") {
      if (loop_stack_.empty()) {
        throw IrError("continue statement outside loop");
      }
      emit("jump " + loop_stack_.back().continue_label);
      return true;
    }

    if (node.children[0]->symbol == "Block") {
      return emitBlock(*node.children[0], true);
    }

    if (node.children[0]->symbol == "Exp") {
      emitExpression(*node.children[0]);
      return false;
    }

    if (node.children[0]->symbol == "SEMICOLON") {
      return false;
    }

    throw IrError("unsupported statement in Koopa generation: " +
                  node.children[0]->symbol);
  }

  bool emitIfStmt(const compiler::parser::ParseNode &node) {
    if (node.children.size() != 6) {
      throw IrError("invalid if statement node");
    }

    Value condition = emitBoolean(emitExpression(*node.children[2]));
    std::string then_label = newLabel("if_then");
    std::string else_label = newLabel("if_else");
    std::string end_label = newLabel("if_end");
    const compiler::parser::ParseNode &then_stmt = *node.children[4];
    const compiler::parser::ParseNode &else_opt = *node.children[5];
    bool has_else = !else_opt.children.empty();

    emit("br " + condition.operand + ", " + then_label + ", " +
         (has_else ? else_label : end_label));

    emitLabel(then_label);
    bool then_returned = emitStmt(then_stmt);
    if (!then_returned) {
      emit("jump " + end_label);
    }

    bool else_returned = false;
    if (has_else) {
      emitLabel(else_label);
      if (else_opt.children.size() != 2 || else_opt.children[0]->symbol != "ELSE" ||
          else_opt.children[1]->symbol != "Stmt") {
        throw IrError("invalid else statement node");
      }
      else_returned = emitStmt(*else_opt.children[1]);
      if (!else_returned) {
        emit("jump " + end_label);
      }
    }

    if (!then_returned || !has_else || !else_returned) {
      emitLabel(end_label);
    }
    return has_else && then_returned && else_returned;
  }

  bool emitWhileStmt(const compiler::parser::ParseNode &node) {
    if (node.children.size() != 5) {
      throw IrError("invalid while statement node");
    }

    std::string entry_label = newLabel("while_entry");
    std::string body_label = newLabel("while_body");
    std::string end_label = newLabel("while_end");

    emit("jump " + entry_label);
    emitLabel(entry_label);
    Value condition = emitBoolean(emitExpression(*node.children[2]));
    emit("br " + condition.operand + ", " + body_label + ", " + end_label);

    emitLabel(body_label);
    loop_stack_.push_back(LoopLabels{end_label, entry_label});
    bool body_terminated = emitStmt(*node.children[4]);
    loop_stack_.pop_back();
    if (!body_terminated) {
      emit("jump " + entry_label);
    }

    emitLabel(end_label);
    return false;
  }

  void collectDeclaration(const compiler::parser::ParseNode &node,
                          bool global_scope) {
    if (node.symbol == "ConstDef") {
      collectConstDef(node);
      return;
    }
    if (node.symbol == "VarDef") {
      collectVarDef(node, global_scope);
      return;
    }
    for (const auto &child : node.children) {
      collectDeclaration(*child, global_scope);
    }
  }

  void collectConstDef(const compiler::parser::ParseNode &node) {
    if (node.children.size() != 4 || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid ConstDef node");
    }
    std::vector<long long> dimensions =
        collectArrayDimensions(*node.children[1]);
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

  void collectVarDef(const compiler::parser::ParseNode &node,
                     bool global_scope) {
    if (node.children.size() < 2 || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid VarDef node");
    }
    const std::string &name = node.children[0]->lexeme;
    if (global_scope) {
      throw IrError("internal error: local function builder saw global variable");
    }
    std::vector<long long> dimensions =
        collectArrayDimensions(*node.children[1]);
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

  void collectLocalArrayDef(const std::string &name,
                            const std::vector<long long> &dimensions,
                            const compiler::parser::ParseNode *initializer,
                            bool assignable) {
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

  Value emitExpression(const compiler::parser::ParseNode &node) {
    if (node.symbol == "INT_CONST") {
      char *end = nullptr;
      long long value = std::strtoll(node.lexeme.c_str(), &end, 0);
      if (end == nullptr || *end != '\0') {
        throw IrError("invalid integer literal: " + node.lexeme);
      }
      return makeConstant(value);
    }

    if (node.symbol == "Number") {
      return emitSingleChild(node);
    }

    if (node.symbol == "LVal") {
      return emitLVal(node);
    }

    if (node.symbol == "Exp" || node.symbol == "ConstExp" ||
        node.symbol == "ConstInitVal" || node.symbol == "InitVal" ||
        node.symbol == "ReturnExpOpt" || node.symbol == "PrimaryExp") {
      if (node.children.size() == 3 && node.children[0]->symbol == "LPAREN") {
        return emitExpression(*node.children[1]);
      }
      return emitSingleChild(node);
    }

    if (node.symbol == "UnaryExp") {
      return emitUnaryExp(node);
    }

    if (node.symbol == "MulExp") {
      return emitBinaryTail(node, "MulExpTail");
    }
    if (node.symbol == "AddExp") {
      return emitBinaryTail(node, "AddExpTail");
    }
    if (node.symbol == "RelExp") {
      return emitBinaryTail(node, "RelExpTail");
    }
    if (node.symbol == "EqExp") {
      return emitBinaryTail(node, "EqExpTail");
    }
    if (node.symbol == "LAndExp") {
      return emitBinaryTail(node, "LAndExpTail");
    }
    if (node.symbol == "LOrExp") {
      return emitBinaryTail(node, "LOrExpTail");
    }

    throw IrError("unsupported expression node: " + node.symbol);
  }

  Value emitSingleChild(const compiler::parser::ParseNode &node) {
    if (node.children.size() != 1) {
      throw IrError("invalid expression wrapper node: " + node.symbol);
    }
    return emitExpression(*node.children[0]);
  }

  Value emitUnaryExp(const compiler::parser::ParseNode &node) {
    if (node.children.size() == 4 && node.children[0]->symbol == "IDENT" &&
        node.children[1]->symbol == "LPAREN") {
      return emitCall(node);
    }
    if (node.children.size() == 1) {
      return emitExpression(*node.children[0]);
    }
    if (node.children.size() == 2) {
      const std::string &op = node.children[0]->children[0]->symbol;
      Value value = emitExpression(*node.children[1]);
      if (op == "PLUS") {
        return value;
      }
      if (value.constant) {
        if (op == "MINUS") {
          return makeConstant(-value.const_value);
        }
        if (op == "NOT") {
          return makeConstant(value.const_value == 0 ? 1 : 0);
        }
      }
      if (op == "MINUS") {
        return emitBinary("MINUS", makeConstant(0), value);
      }
      if (op == "NOT") {
        return emitBinary("EQ", value, makeConstant(0));
      }
    }
    throw IrError("unsupported UnaryExp node");
  }

  Value emitCall(const compiler::parser::ParseNode &node) {
    const std::string &name = node.children[0]->lexeme;
    auto signature = function_signatures_.find(name);
    if (signature == function_signatures_.end()) {
      throw IrError("unknown function: " + name);
    }
    if (signature->second.external) {
      used_external_functions_.insert(name);
    }

    std::vector<const compiler::parser::ParseNode *> arg_nodes;
    collectCallArgumentNodes(*node.children[2], arg_nodes);
    if (arg_nodes.size() != signature->second.parameter_count) {
      throw IrError("argument count mismatch for function: " + name);
    }
    std::vector<Value> args;
    for (size_t i = 0; i < arg_nodes.size(); ++i) {
      std::string parameter_type =
          i < signature->second.parameter_types.size()
              ? signature->second.parameter_types[i]
              : std::string("i32");
      if (startsWith(parameter_type, "*")) {
        args.push_back(emitPointerArgument(*arg_nodes[i], parameter_type));
      } else {
        args.push_back(emitExpression(*arg_nodes[i]));
      }
    }

    std::ostringstream call;
    call << "call @" << name << "(";
    for (size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        call << ", ";
      }
      call << args[i].operand;
    }
    call << ")";

    if (signature->second.return_type == "void") {
      emit(call.str());
      return makeConstant(0);
    }

    std::string result = newTemp();
    emit(result + " = " + call.str());
    return Value{false, 0, result};
  }

  void collectCallArgumentNodes(
      const compiler::parser::ParseNode &node,
      std::vector<const compiler::parser::ParseNode *> &args) {
    if (node.children.empty()) {
      return;
    }
    if (node.symbol == "FuncRParamsOpt") {
      collectCallArgumentNodes(*node.children[0], args);
      return;
    }
    if (node.symbol == "FuncRParams") {
      args.push_back(node.children[0].get());
      collectCallArgumentNodes(*node.children[1], args);
      return;
    }
    if (node.symbol == "FuncRParamsTail") {
      if (node.children.empty()) {
        return;
      }
      if (node.children.size() != 3) {
        throw IrError("invalid FuncRParamsTail node");
      }
      args.push_back(node.children[1].get());
      collectCallArgumentNodes(*node.children[2], args);
      return;
    }
    throw IrError("invalid function argument node: " + node.symbol);
  }

  const compiler::parser::ParseNode *
  unwrapArrayArgument(const compiler::parser::ParseNode &node) {
    if (node.symbol == "LVal") {
      return &node;
    }
    if ((node.symbol == "Exp" || node.symbol == "ConstExp" ||
         node.symbol == "ConstInitVal" || node.symbol == "InitVal" ||
         node.symbol == "ReturnExpOpt" || node.symbol == "Number") &&
        node.children.size() == 1) {
      return unwrapArrayArgument(*node.children[0]);
    }
    if (node.symbol == "PrimaryExp") {
      if (node.children.size() == 1) {
        return unwrapArrayArgument(*node.children[0]);
      }
      if (node.children.size() == 3 && node.children[0]->symbol == "LPAREN") {
        return unwrapArrayArgument(*node.children[1]);
      }
    }
    if (node.symbol == "UnaryExp" && node.children.size() == 1) {
      return unwrapArrayArgument(*node.children[0]);
    }
    if ((node.symbol == "MulExp" || node.symbol == "AddExp" ||
         node.symbol == "RelExp" || node.symbol == "EqExp" ||
         node.symbol == "LAndExp" || node.symbol == "LOrExp") &&
        node.children.size() == 2 && node.children[1]->children.empty()) {
      return unwrapArrayArgument(*node.children[0]);
    }
    return nullptr;
  }

  Value emitPointerArgument(const compiler::parser::ParseNode &node,
                            const std::string &parameter_type) {
    const compiler::parser::ParseNode *lval = unwrapArrayArgument(node);
    if (lval == nullptr || lval->children.empty() ||
        lval->children[0]->symbol != "IDENT") {
      throw IrError("array argument must be an lvalue");
    }
    const Symbol &symbol = lookup(lval->children[0]->lexeme);
    if (symbol.kind != SymbolKind::Variable ||
        (symbol.dimensions.empty() && !symbol.pointer_parameter)) {
      throw IrError("array argument must be an array: " +
                    lval->children[0]->lexeme);
    }

    std::vector<Value> indices = collectLValIndices(*lval);
    std::vector<long long> expected_dimensions =
        parsePointerTypeDimensions(parameter_type);
    std::string pointer = emitDecayedArrayPointer(symbol, indices,
                                                  expected_dimensions);
    return Value{false, 0, pointer};
  }

  Value emitBinaryTail(const compiler::parser::ParseNode &node,
                       const std::string &tail_symbol) {
    if (node.children.size() != 2 || node.children[1]->symbol != tail_symbol) {
      throw IrError("invalid " + node.symbol + " node");
    }
    Value lhs = emitExpression(*node.children[0]);
    if (tail_symbol == "LAndExpTail") {
      if (node.children[1]->children.empty()) {
        return lhs;
      }
      return emitShortCircuitTail(*node.children[1], lhs, false);
    }
    if (tail_symbol == "LOrExpTail") {
      if (node.children[1]->children.empty()) {
        return lhs;
      }
      return emitShortCircuitTail(*node.children[1], lhs, true);
    }
    return emitTail(*node.children[1], lhs);
  }

  Value emitShortCircuitTail(const compiler::parser::ParseNode &node, Value lhs,
                             bool is_or) {
    if (node.children.empty()) {
      return emitBoolean(lhs);
    }
    if (node.children.size() != 3) {
      throw IrError("invalid logical expression tail node: " + node.symbol);
    }

    if (lhs.constant) {
      bool lhs_true = lhs.const_value != 0;
      if ((is_or && lhs_true) || (!is_or && !lhs_true)) {
        return makeConstant(lhs_true ? 1 : 0);
      }
      Value rhs = emitExpression(*node.children[1]);
      return emitShortCircuitTail(*node.children[2], rhs, is_or);
    }

    Value bool_lhs = emitBoolean(lhs);
    std::string result_pointer = newTemp();
    emitLocalAlloc(result_pointer + " = alloc i32");
    emit("store " + std::string(is_or ? "1" : "0") + ", " + result_pointer);

    std::string eval_label = newLabel(is_or ? "or_rhs" : "and_rhs");
    std::string end_label = newLabel(is_or ? "or_end" : "and_end");
    if (is_or) {
      emit("br " + bool_lhs.operand + ", " + end_label + ", " + eval_label);
    } else {
      emit("br " + bool_lhs.operand + ", " + eval_label + ", " + end_label);
    }

    emitLabel(eval_label);
    Value rhs = emitExpression(*node.children[1]);
    Value rhs_value = emitShortCircuitTail(*node.children[2], rhs, is_or);
    emit("store " + rhs_value.operand + ", " + result_pointer);
    emit("jump " + end_label);

    emitLabel(end_label);
    std::string loaded = newTemp();
    emit(loaded + " = load " + result_pointer);
    return Value{false, 0, loaded};
  }

  Value emitTail(const compiler::parser::ParseNode &node, Value lhs) {
    if (node.children.empty()) {
      return lhs;
    }
    if (node.children.size() != 3) {
      throw IrError("invalid expression tail node: " + node.symbol);
    }

    const std::string &op = node.children[0]->symbol;
    Value rhs = emitExpression(*node.children[1]);
    Value combined = emitBinary(op, lhs, rhs);
    return emitTail(*node.children[2], combined);
  }

  Value emitBinary(const std::string &op, Value lhs, Value rhs) {
    if (lhs.constant && rhs.constant) {
      return makeConstant(foldBinary(op, lhs.const_value, rhs.const_value));
    }
    if (op == "AND" || op == "OR") {
      Value bool_lhs = emitBoolean(lhs);
      Value bool_rhs = emitBoolean(rhs);
      std::string result = newTemp();
      emit(result + " = " + koopaOp(op) + " " + bool_lhs.operand + ", " +
           bool_rhs.operand);
      return Value{false, 0, result};
    }
    std::string result = newTemp();
    emit(result + " = " + koopaOp(op) + " " + lhs.operand + ", " +
         rhs.operand);
    return Value{false, 0, result};
  }

  Value emitBoolean(Value value) {
    if (value.constant) {
      return makeConstant(value.const_value != 0 ? 1 : 0);
    }
    std::string result = newTemp();
    emit(result + " = ne " + value.operand + ", 0");
    return Value{false, 0, result};
  }

  Value emitLVal(const compiler::parser::ParseNode &node) {
    if (node.children.empty() || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid LVal node");
    }

    const Symbol &symbol = lookup(node.children[0]->lexeme);
    if (symbol.kind == SymbolKind::Constant) {
      return makeConstant(symbol.const_value);
    }
    if (!symbol.dimensions.empty() || symbol.pointer_parameter) {
      std::vector<Value> indices = collectLValIndices(node);
      std::string pointer = emitArrayAccessPointer(symbol, indices);
      size_t required_indices = symbol.dimensions.size() +
                                (symbol.pointer_parameter ? 1 : 0);
      if (indices.size() < required_indices) {
        return Value{false, 0, pointer};
      }
      std::string loaded = newTemp();
      emit(loaded + " = load " + pointer);
      return Value{false, 0, loaded};
    }

    std::string loaded = newTemp();
    emit(loaded + " = load " + symbol.pointer);
    return Value{false, 0, loaded};
  }

  std::string lookupVariablePointer(const compiler::parser::ParseNode &lval) {
    if (lval.children.empty() || lval.children[0]->symbol != "IDENT") {
      throw IrError("invalid assignment LVal");
    }

    const Symbol &symbol = lookup(lval.children[0]->lexeme);
    if (symbol.kind != SymbolKind::Variable || !symbol.assignable) {
      throw IrError("cannot assign to constant: " + lval.children[0]->lexeme);
    }
    if (!symbol.dimensions.empty() || symbol.pointer_parameter) {
      std::vector<Value> indices = collectLValIndices(lval);
      size_t required_indices = symbol.dimensions.size() +
                                (symbol.pointer_parameter ? 1 : 0);
      if (indices.size() != required_indices) {
        throw IrError("array assignment must index every dimension: " +
                      lval.children[0]->lexeme);
      }
      return emitArrayAccessPointer(symbol, indices);
    }
    return symbol.pointer;
  }

  std::string emitArrayAccessPointer(const Symbol &symbol,
                                     const std::vector<Value> &indices) {
    if (indices.empty()) {
      if (symbol.pointer_parameter) {
        return symbol.pointer;
      }
      return emitArrayElementPointer(symbol.pointer, symbol.dimensions,
                                     std::vector<Value>{makeConstant(0)}, false);
    }
    return emitArrayElementPointer(symbol.pointer, symbol.dimensions, indices,
                                   symbol.pointer_parameter);
  }

  std::string emitDecayedArrayPointer(
      const Symbol &symbol, const std::vector<Value> &indices,
      const std::vector<long long> &expected_dimensions) {
    size_t consumed_dimensions = 0;
    std::string pointer;
    if (symbol.pointer_parameter) {
      if (indices.empty()) {
        pointer = symbol.pointer;
      } else {
        pointer = emitArrayElementPointer(symbol.pointer, symbol.dimensions,
                                          indices, true);
        consumed_dimensions = indices.size() - 1;
      }
    } else {
      std::vector<Value> actual_indices = indices;
      if (actual_indices.empty()) {
        actual_indices.push_back(makeConstant(0));
      }
      pointer = emitArrayElementPointer(symbol.pointer, symbol.dimensions,
                                        actual_indices, false);
      consumed_dimensions = actual_indices.size();
    }

    if (consumed_dimensions > symbol.dimensions.size()) {
      throw IrError("too many indices for array argument");
    }
    std::vector<long long> remaining(symbol.dimensions.begin() +
                                         consumed_dimensions,
                                     symbol.dimensions.end());
    while (remaining.size() > expected_dimensions.size()) {
      std::string next = newTemp();
      emit(next + " = getelemptr " + pointer + ", 0");
      pointer = next;
      remaining.erase(remaining.begin());
    }
    if (remaining != expected_dimensions) {
      throw IrError("array argument type mismatch");
    }
    return pointer;
  }

  std::vector<Value> collectLValIndices(
      const compiler::parser::ParseNode &lval) {
    std::vector<Value> indices;
    if (lval.children.size() < 2) {
      return indices;
    }
    const compiler::parser::ParseNode *dims = lval.children[1].get();
    while (dims != nullptr && !dims->children.empty()) {
      if (dims->children.size() != 4) {
        throw IrError("invalid LVal array dimensions");
      }
      indices.push_back(emitExpression(*dims->children[1]));
      dims = dims->children[3].get();
    }
    return indices;
  }

  std::string emitArrayElementPointer(const std::string &base_pointer,
                                      const std::vector<long long> &dimensions,
                                      long long linear_index) {
    std::vector<Value> indices;
    for (size_t i = 0; i < dimensions.size(); ++i) {
      long long stride = elementCount(dimensions, i + 1);
      indices.push_back(makeConstant(linear_index / stride));
      linear_index %= stride;
    }
    return emitArrayElementPointer(base_pointer, dimensions, indices, false);
  }

  std::string emitArrayElementPointer(const std::string &base_pointer,
                                      const std::vector<long long> &dimensions,
                                      const std::vector<Value> &indices,
                                      bool first_getptr) {
    size_t max_indices = dimensions.size() + (first_getptr ? 1 : 0);
    if (indices.empty() || indices.size() > max_indices) {
      throw IrError("invalid array index count");
    }
    std::string pointer = base_pointer;
    for (size_t i = 0; i < indices.size(); ++i) {
      std::string next = newTemp();
      emit(next + " = " + std::string(first_getptr && i == 0 ? "getptr "
                                                             : "getelemptr ") +
           pointer + ", " + indices[i].operand);
      pointer = next;
    }
    return pointer;
  }

  std::vector<long long> collectArrayDimensions(
      const compiler::parser::ParseNode &node) {
    std::vector<long long> dimensions;
    const compiler::parser::ParseNode *current = &node;
    while (current != nullptr && !current->children.empty()) {
      if (current->children.size() != 4) {
        throw IrError("invalid array dimensions");
      }
      Value value = emitExpression(*current->children[1]);
      long long dimension = expectConstant(value, "array dimension");
      if (dimension <= 0) {
        throw IrError("array dimension must be positive");
      }
      dimensions.push_back(dimension);
      current = current->children[3].get();
    }
    return dimensions;
  }

  void collectInitializerChildren(const compiler::parser::ParseNode &node,
                                  std::vector<const compiler::parser::ParseNode *> &out) {
    if (node.children.empty()) {
      return;
    }
    if (node.symbol == "InitValListOpt" || node.symbol == "ConstInitValListOpt") {
      out.push_back(node.children[0].get());
      collectInitializerChildren(*node.children[1], out);
      return;
    }
    if (node.symbol == "InitValListTail" ||
        node.symbol == "ConstInitValListTail") {
      if (node.children.empty()) {
        return;
      }
      if (node.children.size() == 1) {
        return;
      }
      out.push_back(node.children[1].get());
      collectInitializerChildren(*node.children[2], out);
      return;
    }
    throw IrError("invalid array initializer list: " + node.symbol);
  }

  bool isInitializerList(const compiler::parser::ParseNode &node) const {
    return !node.children.empty() && node.children[0]->symbol == "LBRACE";
  }

  long long flattenRuntimeInitializer(
      const compiler::parser::ParseNode &node,
      const std::vector<long long> &dimensions, size_t depth, long long begin,
      std::vector<std::pair<long long, Value>> &entries) {
    if (!isInitializerList(node)) {
      entries.push_back({begin, emitExpression(node)});
      return begin + 1;
    }

    std::vector<const compiler::parser::ParseNode *> children;
    collectInitializerChildren(*node.children[1], children);
    long long cursor = begin;
    long long limit = begin + elementCount(dimensions, depth);
    for (const compiler::parser::ParseNode *child : children) {
      if (isInitializerList(*child) && depth + 1 < dimensions.size()) {
        size_t child_depth = depth + 1;
        for (; child_depth < dimensions.size(); ++child_depth) {
          long long sub_size = elementCount(dimensions, child_depth);
          if ((cursor - begin) % sub_size == 0) {
            break;
          }
        }
        cursor =
            flattenRuntimeInitializer(*child, dimensions, child_depth, cursor, entries);
      } else {
        cursor =
            flattenRuntimeInitializer(*child, dimensions, dimensions.size(), cursor,
                                      entries);
      }
      if (cursor > limit) {
        throw IrError("too many array initializer values");
      }
    }
    return limit;
  }

  Value makeConstant(long long value) const {
    return Value{true, value, toOperand(value)};
  }

  void emit(std::string instruction) {
    if (isTerminator(instruction)) {
      block_terminated_ = true;
    }
    instructions_.push_back(std::move(instruction));
  }

  void emitLocalAlloc(std::string instruction) {
    entry_allocs_.push_back(std::move(instruction));
  }

  void emitLabel(const std::string &label) {
    block_terminated_ = false;
    emit(label + ":");
  }

  bool isTerminator(const std::string &instruction) const {
    return startsWith(instruction, "br ") || startsWith(instruction, "jump ") ||
           startsWith(instruction, "ret ");
  }

  struct Parameter {
    std::string source_name;
    std::string koopa_name;
    std::string pointer;
    std::string type;
  };

  void collectParameters() {
    const compiler::parser::ParseNode *params_opt =
        findDirectChild(function_, "FuncFParamsOpt");
    if (params_opt == nullptr || params_opt->children.empty() ||
        params_opt->children[0]->symbol == "VOID") {
      return;
    }

    std::vector<const compiler::parser::ParseNode *> param_nodes;
    collectNodes(*params_opt, "FuncFParam", param_nodes);
    for (const compiler::parser::ParseNode *param : param_nodes) {
      const compiler::parser::ParseNode *ident = findDirectChild(*param, "IDENT");
      if (ident == nullptr || ident->lexeme.empty()) {
        throw IrError("invalid function parameter");
      }
      std::string koopa_name = newParameterValue(ident->lexeme);
      std::vector<long long> dimensions = collectFunctionParameterDimensions(*param);
      if (!dimensions.empty() || hasNonEmptyChild(*param, "FuncFParamArrayOpt")) {
        std::string type = "*" + arrayType(dimensions);
        define(ident->lexeme,
               Symbol{SymbolKind::Variable, 0, koopa_name, dimensions, true, true});
        parameters_.push_back(Parameter{ident->lexeme, koopa_name, "", type});
        continue;
      }
      std::string pointer = newNamedValue(ident->lexeme);
      emitLocalAlloc(pointer + " = alloc i32");
      define(ident->lexeme,
             Symbol{SymbolKind::Variable, 0, pointer, {}, true, false});
      parameters_.push_back(Parameter{ident->lexeme, koopa_name, pointer, "i32"});
    }
  }

  std::string newTemp() { return "%" + std::to_string(temp_id_++); }

  std::vector<long long> collectFunctionParameterDimensions(
      const compiler::parser::ParseNode &param) {
    std::vector<long long> dimensions;
    const compiler::parser::ParseNode *array_opt =
        findDirectChild(param, "FuncFParamArrayOpt");
    if (array_opt == nullptr || array_opt->children.empty()) {
      return dimensions;
    }
    const compiler::parser::ParseNode *current =
        findDirectChild(*array_opt, "FuncFParamArrayDims");
    while (current != nullptr && !current->children.empty()) {
      if (current->children.size() != 4) {
        throw IrError("invalid function parameter array dimensions");
      }
      long long dimension =
          expectConstant(emitExpression(*current->children[1]),
                         "function parameter array dimension");
      if (dimension <= 0) {
        throw IrError("array dimension must be positive");
      }
      dimensions.push_back(dimension);
      current = current->children[3].get();
    }
    return dimensions;
  }

  std::string newLabel(const std::string &prefix) {
    return "%" + prefix + "_" + std::to_string(label_id_++);
  }

  std::string newGlobalValue(const std::string &name) {
    std::string base = "@" + name;
    std::string candidate = base;
    int suffix = 0;
    while (used_values_.find(candidate) != used_values_.end()) {
      candidate = base + "_" + std::to_string(++suffix);
    }
    used_values_.insert(candidate);
    return candidate;
  }

  std::string newNamedValue(const std::string &name) {
    std::string base = "%" + name;
    std::string candidate = base;
    int suffix = 0;
    while (used_values_.find(candidate) != used_values_.end()) {
      candidate = base + "_" + std::to_string(++suffix);
    }
    used_values_.insert(candidate);
    return candidate;
  }

  std::string newParameterValue(const std::string &name) {
    std::string base = "@" + function_name_ + "_" + name;
    std::string candidate = base;
    int suffix = 0;
    while (used_values_.find(candidate) != used_values_.end()) {
      candidate = base + "_" + std::to_string(++suffix);
    }
    used_values_.insert(candidate);
    return candidate;
  }

  void define(const std::string &name, Symbol symbol) {
    if (scopes_.back().find(name) != scopes_.back().end()) {
      throw IrError("duplicate identifier in current scope: " + name);
    }
    scopes_.back()[name] = std::move(symbol);
  }

  const Symbol &lookup(const std::string &name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
      auto found = scope->find(name);
      if (found != scope->end()) {
        return found->second;
      }
    }
    throw IrError("unknown identifier: " + name);
  }

  const compiler::parser::ParseNode &function_;
  std::map<std::string, FunctionSignature> function_signatures_;
  std::set<std::string> &used_external_functions_;
  std::string function_name_;
  std::string return_type_ = "i32";
  std::vector<Parameter> parameters_;
  std::vector<std::string> entry_allocs_;
  std::vector<std::string> instructions_;
  std::vector<std::map<std::string, Symbol>> scopes_;
  std::set<std::string> used_values_;
  struct LoopLabels {
    std::string break_label;
    std::string continue_label;
  };
  std::vector<LoopLabels> loop_stack_;
  int temp_id_ = 0;
  int label_id_ = 0;
  bool block_terminated_ = false;
  bool returned_ = false;
};

std::string findFunctionName(const compiler::parser::ParseNode &function) {
  const compiler::parser::ParseNode *ident = findDirectChild(function, "IDENT");
  if (ident == nullptr || ident->lexeme.empty()) {
    ident = findFirst(function, "IDENT");
  }
  if (ident == nullptr || ident->lexeme.empty()) {
    throw IrError("cannot find function name in AST");
  }
  return ident->lexeme;
}

std::string findFunctionReturnType(const compiler::parser::ParseNode &function) {
  const compiler::parser::ParseNode *func_type =
      findDirectChild(function, "FuncType");
  if (func_type != nullptr && !func_type->children.empty() &&
      func_type->children[0]->symbol == "VOID") {
    return "void";
  }
  return "i32";
}

class ProgramBuilder {
public:
  std::string generate(const compiler::parser::ParseNode &ast) {
    collectGlobalDeclarations(ast);
    collectFunctionSignatures(ast);

    std::vector<const compiler::parser::ParseNode *> functions;
    collectNodes(ast, "FuncDef", functions);
    if (functions.empty()) {
      throw IrError("cannot find function definition in AST");
    }

    std::vector<std::string> function_outputs;
    for (const compiler::parser::ParseNode *function : functions) {
      FunctionBuilder builder(*function, global_symbols_, function_signatures_,
                              used_values_, used_external_functions_);
      function_outputs.push_back(builder.generate());
    }

    std::ostringstream output;
    for (const std::string &line : global_instructions_) {
      output << line << '\n';
    }
    for (const std::string &name : used_external_functions_) {
      const FunctionSignature &signature = function_signatures_.at(name);
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
    if (!global_instructions_.empty() || !used_external_functions_.empty()) {
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

private:
  void collectFunctionSignatures(const compiler::parser::ParseNode &ast) {
    function_signatures_["getint"] = FunctionSignature{"i32", 0, true, {}};
    function_signatures_["getch"] = FunctionSignature{"i32", 0, true, {}};
    function_signatures_["getarray"] =
        FunctionSignature{"i32", 1, true, {"*i32"}};
    function_signatures_["putint"] =
        FunctionSignature{"void", 1, true, {"i32"}};
    function_signatures_["putch"] =
        FunctionSignature{"void", 1, true, {"i32"}};
    function_signatures_["putarray"] =
        FunctionSignature{"void", 2, true, {"i32", "*i32"}};
    function_signatures_["starttime"] = FunctionSignature{"void", 0, true, {}};
    function_signatures_["stoptime"] = FunctionSignature{"void", 0, true, {}};
    used_values_.insert("@getint");
    used_values_.insert("@getch");
    used_values_.insert("@getarray");
    used_values_.insert("@putint");
    used_values_.insert("@putch");
    used_values_.insert("@putarray");
    used_values_.insert("@starttime");
    used_values_.insert("@stoptime");

    std::vector<const compiler::parser::ParseNode *> functions;
    collectNodes(ast, "FuncDef", functions);
    for (const compiler::parser::ParseNode *function : functions) {
      std::string name = findFunctionName(*function);
      if (function_signatures_.find(name) != function_signatures_.end()) {
        throw IrError("duplicate function: " + name);
      }
      if (global_symbols_.find(name) != global_symbols_.end()) {
        throw IrError("duplicate global identifier and function: " + name);
      }
      function_signatures_[name] =
          FunctionSignature{findFunctionReturnType(*function),
                            countFunctionParameters(*function), false,
                            collectFunctionParameterTypes(*function)};
      used_values_.insert("@" + name);
    }
  }

  size_t countFunctionParameters(const compiler::parser::ParseNode &function) {
    const compiler::parser::ParseNode *params_opt =
        findDirectChild(function, "FuncFParamsOpt");
    if (params_opt == nullptr || params_opt->children.empty() ||
        params_opt->children[0]->symbol == "VOID") {
      return 0;
    }
    std::vector<const compiler::parser::ParseNode *> params;
    collectNodes(*params_opt, "FuncFParam", params);
    return params.size();
  }

  std::vector<std::string> collectFunctionParameterTypes(
      const compiler::parser::ParseNode &function) {
    const compiler::parser::ParseNode *params_opt =
        findDirectChild(function, "FuncFParamsOpt");
    if (params_opt == nullptr || params_opt->children.empty() ||
        params_opt->children[0]->symbol == "VOID") {
      return {};
    }
    std::vector<const compiler::parser::ParseNode *> params;
    collectNodes(*params_opt, "FuncFParam", params);
    std::vector<std::string> types;
    for (const compiler::parser::ParseNode *param : params) {
      const compiler::parser::ParseNode *array_opt =
          findDirectChild(*param, "FuncFParamArrayOpt");
      if (array_opt == nullptr || array_opt->children.empty()) {
        types.push_back("i32");
        continue;
      }
      std::vector<long long> dimensions;
      const compiler::parser::ParseNode *current =
          findDirectChild(*array_opt, "FuncFParamArrayDims");
      while (current != nullptr && !current->children.empty()) {
        long long dimension =
            expectConstant(emitGlobalExpression(*current->children[1]),
                           "function parameter array dimension");
        dimensions.push_back(dimension);
        current = current->children[3].get();
      }
      types.push_back("*" + arrayType(dimensions));
    }
    return types;
  }

  void collectGlobalDeclarations(const compiler::parser::ParseNode &node) {
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

  void collectDeclaration(const compiler::parser::ParseNode &node) {
    if (node.symbol == "ConstDef") {
      collectConstDef(node);
      return;
    }
    if (node.symbol == "VarDef") {
      collectVarDef(node);
      return;
    }
    for (const auto &child : node.children) {
      collectDeclaration(*child);
    }
  }

  void collectConstDef(const compiler::parser::ParseNode &node) {
    if (node.children.size() != 4 || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid ConstDef node");
    }
    std::vector<long long> dimensions =
        collectGlobalArrayDimensions(*node.children[1]);
    if (!dimensions.empty()) {
      std::string pointer = newGlobalValue(node.children[0]->lexeme);
      std::vector<long long> values(elementCount(dimensions), 0);
      fillGlobalInitializer(*node.children[3], dimensions, 0, 0, values);
      global_instructions_.push_back("global " + pointer + " = alloc " +
                                     arrayType(dimensions) + ", " +
                                     formatArrayInitializer(values, dimensions, 0, 0));
      defineGlobal(node.children[0]->lexeme,
                   Symbol{SymbolKind::Variable, 0, pointer, dimensions, false,
                          false});
      return;
    }
    long long value =
        expectConstant(emitGlobalExpression(*node.children[3]), "const initializer");
    defineGlobal(node.children[0]->lexeme,
                 Symbol{SymbolKind::Constant, value, "", {}, false, false});
  }

  void collectVarDef(const compiler::parser::ParseNode &node) {
    if (node.children.size() < 2 || node.children[0]->symbol != "IDENT") {
      throw IrError("invalid VarDef node");
    }
    long long value = 0;
    bool has_initializer = false;
    std::vector<long long> dimensions =
        collectGlobalArrayDimensions(*node.children[1]);
    if (!dimensions.empty()) {
      std::string pointer = newGlobalValue(node.children[0]->lexeme);
      std::vector<long long> values(elementCount(dimensions), 0);
      if (node.children.size() >= 3 && !node.children[2]->children.empty()) {
        fillGlobalInitializer(*node.children[2]->children[1], dimensions, 0, 0,
                              values);
        global_instructions_.push_back("global " + pointer + " = alloc " +
                                       arrayType(dimensions) + ", " +
                                       formatArrayInitializer(values, dimensions, 0,
                                                              0));
      } else {
        global_instructions_.push_back("global " + pointer + " = alloc " +
                                       arrayType(dimensions) + ", zeroinit");
      }
      defineGlobal(node.children[0]->lexeme,
                   Symbol{SymbolKind::Variable, 0, pointer, dimensions, true,
                          false});
      return;
    }
    if (node.children.size() >= 3 && !node.children[2]->children.empty()) {
      has_initializer = true;
      value = expectConstant(emitGlobalExpression(*node.children[2]->children[1]),
                             "global variable initializer");
    }
    std::string pointer = newGlobalValue(node.children[0]->lexeme);
    global_instructions_.push_back("global " + pointer + " = alloc i32, " +
                                   (has_initializer ? std::to_string(value)
                                                    : "zeroinit"));
    defineGlobal(node.children[0]->lexeme,
                 Symbol{SymbolKind::Variable, 0, pointer, {}, true, false});
  }

  Value emitGlobalExpression(const compiler::parser::ParseNode &node) {
    if (node.symbol == "INT_CONST") {
      char *end = nullptr;
      long long value = std::strtoll(node.lexeme.c_str(), &end, 0);
      if (end == nullptr || *end != '\0') {
        throw IrError("invalid integer literal: " + node.lexeme);
      }
      return Value{true, value, toOperand(value)};
    }
    if (node.symbol == "LVal") {
      const Symbol &symbol = lookupGlobal(node.children[0]->lexeme);
      if (symbol.kind != SymbolKind::Constant) {
        throw IrError("global initializer must be constant");
      }
      return Value{true, symbol.const_value, toOperand(symbol.const_value)};
    }
    if (node.symbol == "Number" || node.symbol == "Exp" ||
        node.symbol == "ConstExp" || node.symbol == "ConstInitVal" ||
        node.symbol == "InitVal" || node.symbol == "PrimaryExp") {
      if (node.children.size() == 3 && node.children[0]->symbol == "LPAREN") {
        return emitGlobalExpression(*node.children[1]);
      }
      if (node.children.size() != 1) {
        throw IrError("invalid constant expression node: " + node.symbol);
      }
      return emitGlobalExpression(*node.children[0]);
    }
    if (node.symbol == "UnaryExp") {
      if (node.children.size() == 1) {
        return emitGlobalExpression(*node.children[0]);
      }
      if (node.children.size() == 2) {
        const std::string &op = node.children[0]->children[0]->symbol;
        Value value = emitGlobalExpression(*node.children[1]);
        if (op == "PLUS") {
          return value;
        }
        if (op == "MINUS") {
          return Value{true, -value.const_value, toOperand(-value.const_value)};
        }
        if (op == "NOT") {
          long long folded = value.const_value == 0 ? 1 : 0;
          return Value{true, folded, toOperand(folded)};
        }
      }
    }
    if (node.symbol == "MulExp" || node.symbol == "AddExp" ||
        node.symbol == "RelExp" || node.symbol == "EqExp" ||
        node.symbol == "LAndExp" || node.symbol == "LOrExp") {
      return emitGlobalBinaryTail(node);
    }
    throw IrError("unsupported global constant expression: " + node.symbol);
  }

  Value emitGlobalBinaryTail(const compiler::parser::ParseNode &node) {
    if (node.children.size() != 2) {
      throw IrError("invalid constant expression node: " + node.symbol);
    }
    Value lhs = emitGlobalExpression(*node.children[0]);
    return emitGlobalTail(*node.children[1], lhs);
  }

  Value emitGlobalTail(const compiler::parser::ParseNode &node, Value lhs) {
    if (node.children.empty()) {
      return lhs;
    }
    if (node.children.size() != 3) {
      throw IrError("invalid constant expression tail node: " + node.symbol);
    }
    Value rhs = emitGlobalExpression(*node.children[1]);
    long long value = foldBinary(node.children[0]->symbol, lhs.const_value,
                                 rhs.const_value);
    return emitGlobalTail(*node.children[2],
                          Value{true, value, toOperand(value)});
  }

  std::vector<long long> collectGlobalArrayDimensions(
      const compiler::parser::ParseNode &node) {
    std::vector<long long> dimensions;
    const compiler::parser::ParseNode *current = &node;
    while (current != nullptr && !current->children.empty()) {
      if (current->children.size() != 4) {
        throw IrError("invalid global array dimensions");
      }
      long long dimension =
          expectConstant(emitGlobalExpression(*current->children[1]),
                         "global array dimension");
      if (dimension <= 0) {
        throw IrError("array dimension must be positive");
      }
      dimensions.push_back(dimension);
      current = current->children[3].get();
    }
    return dimensions;
  }

  void collectInitializerChildren(const compiler::parser::ParseNode &node,
                                  std::vector<const compiler::parser::ParseNode *> &out) {
    if (node.children.empty()) {
      return;
    }
    if (node.symbol == "InitValListOpt" || node.symbol == "ConstInitValListOpt") {
      out.push_back(node.children[0].get());
      collectInitializerChildren(*node.children[1], out);
      return;
    }
    if (node.symbol == "InitValListTail" ||
        node.symbol == "ConstInitValListTail") {
      if (node.children.empty() || node.children.size() == 1) {
        return;
      }
      out.push_back(node.children[1].get());
      collectInitializerChildren(*node.children[2], out);
      return;
    }
    throw IrError("invalid global array initializer list: " + node.symbol);
  }

  bool isInitializerList(const compiler::parser::ParseNode &node) const {
    return !node.children.empty() && node.children[0]->symbol == "LBRACE";
  }

  long long fillGlobalInitializer(const compiler::parser::ParseNode &node,
                                  const std::vector<long long> &dimensions,
                                  size_t depth, long long begin,
                                  std::vector<long long> &values) {
    if (!isInitializerList(node)) {
      if (begin < 0 || begin >= static_cast<long long>(values.size())) {
        throw IrError("too many global array initializer values");
      }
      values[begin] =
          expectConstant(emitGlobalExpression(node), "global array initializer");
      return begin + 1;
    }

    std::vector<const compiler::parser::ParseNode *> children;
    collectInitializerChildren(*node.children[1], children);
    long long cursor = begin;
    long long limit = begin + elementCount(dimensions, depth);
    for (const compiler::parser::ParseNode *child : children) {
      if (isInitializerList(*child) && depth + 1 < dimensions.size()) {
        size_t child_depth = depth + 1;
        for (; child_depth < dimensions.size(); ++child_depth) {
          long long sub_size = elementCount(dimensions, child_depth);
          if ((cursor - begin) % sub_size == 0) {
            break;
          }
        }
        cursor = fillGlobalInitializer(*child, dimensions, child_depth, cursor,
                                       values);
      } else {
        cursor = fillGlobalInitializer(*child, dimensions, dimensions.size(),
                                       cursor, values);
      }
      if (cursor > limit) {
        throw IrError("too many global array initializer values");
      }
    }
    return limit;
  }

  std::string formatArrayInitializer(const std::vector<long long> &values,
                                     const std::vector<long long> &dimensions,
                                     size_t depth, long long begin) {
    if (depth == dimensions.size()) {
      return std::to_string(values[begin]);
    }
    long long sub_size = elementCount(dimensions, depth + 1);
    std::ostringstream output;
    output << "{";
    for (long long i = 0; i < dimensions[depth]; ++i) {
      if (i != 0) {
        output << ", ";
      }
      output << formatArrayInitializer(values, dimensions, depth + 1,
                                       begin + i * sub_size);
    }
    output << "}";
    return output.str();
  }

  std::string newGlobalValue(const std::string &name) {
    std::string base = "@" + name;
    std::string candidate = base;
    int suffix = 0;
    while (used_values_.find(candidate) != used_values_.end()) {
      candidate = base + "_" + std::to_string(++suffix);
    }
    used_values_.insert(candidate);
    return candidate;
  }

  void defineGlobal(const std::string &name, Symbol symbol) {
    if (global_symbols_.find(name) != global_symbols_.end()) {
      throw IrError("duplicate identifier in global scope: " + name);
    }
    global_symbols_[name] = std::move(symbol);
  }

  const Symbol &lookupGlobal(const std::string &name) const {
    auto found = global_symbols_.find(name);
    if (found == global_symbols_.end()) {
      throw IrError("unknown global identifier: " + name);
    }
    return found->second;
  }

  std::vector<std::string> global_instructions_;
  std::map<std::string, Symbol> global_symbols_;
  std::map<std::string, FunctionSignature> function_signatures_;
  std::set<std::string> used_external_functions_;
  std::set<std::string> used_values_;
};

} // namespace

std::string KoopaGenerator::generate(
    const compiler::parser::ParseNode &ast) const {
  return ProgramBuilder().generate(ast);
}

void KoopaGenerator::generate(const compiler::parser::ParseNode &ast,
                              std::ostream &output) const {
  output << generate(ast);
}

} // namespace compiler::ir
