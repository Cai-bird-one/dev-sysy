#include "compiler/ir/sdt/sdt.h"

#include <utility>

namespace compiler::ir::sdt {

SyntaxDirectedTranslator::SyntaxDirectedTranslator() {
  default_rule_ = [](const compiler::parser::ParseNode &node,
                     const SyntaxDirectedTranslator &translator) {
    AttributeSet result;
    if (!node.lexeme.empty()) {
      result.set<std::string>("lexeme", node.lexeme);
    }
    for (const auto &child : node.children) {
      translator.translate(*child);
    }
    return result;
  };
}

void SyntaxDirectedTranslator::registerRule(int production_id, Rule rule) {
  rules_[production_id] = std::move(rule);
}

void SyntaxDirectedTranslator::setDefaultRule(Rule rule) {
  default_rule_ = std::move(rule);
}

AttributeSet
SyntaxDirectedTranslator::translate(
    const compiler::parser::ParseNode &node) const {
  auto found = rules_.find(node.production_id);
  if (found != rules_.end()) {
    return found->second(node, *this);
  }
  return default_rule_(node, *this);
}

} // namespace compiler::ir::sdt
