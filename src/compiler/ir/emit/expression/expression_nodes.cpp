#include "compiler/ir/emit/expression/expression_nodes.h"

namespace compiler::ir {

bool isExpressionWrapper(const std::string &symbol) {
  return symbol == "Number" || symbol == "Exp" || symbol == "ConstExp" ||
         symbol == "ConstInitVal" || symbol == "InitVal" ||
         symbol == "ReturnExpOpt" || symbol == "PrimaryExp";
}

bool isBinaryExpression(const std::string &symbol) {
  return symbol == "MulExp" || symbol == "AddExp" || symbol == "RelExp" ||
         symbol == "EqExp" || symbol == "LAndExp" || symbol == "LOrExp";
}

std::string binaryTailSymbol(const std::string &symbol) {
  if (symbol == "MulExp") {
    return "MulExpTail";
  }
  if (symbol == "AddExp") {
    return "AddExpTail";
  }
  if (symbol == "RelExp") {
    return "RelExpTail";
  }
  if (symbol == "EqExp") {
    return "EqExpTail";
  }
  if (symbol == "LAndExp") {
    return "LAndExpTail";
  }
  if (symbol == "LOrExp") {
    return "LOrExpTail";
  }
  return "";
}

} // namespace compiler::ir
