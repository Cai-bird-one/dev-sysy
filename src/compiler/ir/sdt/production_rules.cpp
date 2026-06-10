#include "compiler/ir/sdt/production_rules.h"

#include "compiler/ir/koopa_generator.h"
#include "compiler/parser/grammar_rules.h"

#include <utility>
#include <vector>

namespace compiler::ir::sdt {

int findProductionId(const std::string &lhs,
                     std::initializer_list<std::string> rhs) {
  const compiler::parser::Grammar &grammar = compiler::parser::defaultGrammar();
  std::vector<std::string> expected(rhs);
  for (size_t i = 0; i < grammar.productions.size(); ++i) {
    const compiler::parser::Production &production = grammar.productions[i];
    if (production.lhs == lhs && production.rhs == expected) {
      return static_cast<int>(i);
    }
  }
  throw IrError("cannot find grammar production for SDT rule: " + lhs);
}

void registerProductionRule(SyntaxDirectedTranslator &translator,
                            const std::string &lhs,
                            std::initializer_list<std::string> rhs,
                            SyntaxDirectedTranslator::Rule rule) {
  translator.registerRule(findProductionId(lhs, rhs), std::move(rule));
}

} // namespace compiler::ir::sdt
