#include "compiler/parser/parser.h"

#include <ostream>
#include <utility>

namespace compiler::parser {

ParseNode::ParseNode(std::string symbol) : symbol(std::move(symbol)) {}

ParseNode::ParseNode(std::string symbol, std::string lexeme)
    : symbol(std::move(symbol)), lexeme(std::move(lexeme)) {}

void printParseTree(const ParseNode &node, std::ostream &output, int indent) {
  output << std::string(static_cast<size_t>(indent), ' ') << node.symbol;
  if (!node.lexeme.empty()) {
    output << " \"" << node.lexeme << '"';
  }
  output << '\n';
  for (const auto &child : node.children) {
    printParseTree(*child, output, indent + 2);
  }
}

} // namespace compiler::parser
