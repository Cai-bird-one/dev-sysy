#pragma once

#include "compiler/parser/parser.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace compiler::parser {

const char *endSymbol();
const char *epsilonSymbol();

struct ParserAnalysis {
  std::vector<Production> productions;
  std::set<std::string> terminals;
  std::set<std::string> nonterminals;
  std::map<std::string, std::map<std::string, std::vector<int>>> parse_table;
};

ParserAnalysis analyzeGrammar(const Grammar &grammar,
                              const std::set<std::string> &available_tokens);

} // namespace compiler::parser
