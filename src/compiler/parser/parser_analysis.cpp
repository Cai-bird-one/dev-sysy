#include "compiler/parser/parser_analysis.h"

#include <algorithm>
#include <cctype>

namespace compiler::parser {
namespace {

constexpr const char *kEndSymbol = "$";
constexpr const char *kEpsilon = "<epsilon>";

using SymbolSet = std::set<std::string>;

std::set<std::string> collectNonterminals(
    const std::vector<Production> &productions) {
  std::set<std::string> nonterminals;
  for (const Production &production : productions) {
    nonterminals.insert(production.lhs);
  }
  return nonterminals;
}

bool isTerminalName(const std::string &symbol) {
  if (symbol.empty()) {
    return false;
  }
  unsigned char first = static_cast<unsigned char>(symbol.front());
  if (!(std::isupper(first) || symbol.front() == '_')) {
    return false;
  }
  for (char ch : symbol) {
    unsigned char uch = static_cast<unsigned char>(ch);
    if (!(std::isupper(uch) || std::isdigit(uch) || ch == '_')) {
      return false;
    }
  }
  return true;
}

std::set<std::string>
inferTerminals(const Grammar &grammar, const std::set<std::string> &nonterminals,
               const std::set<std::string> &available_tokens) {
  if (available_tokens.empty()) {
    throw ParserError("parser requires available lexer token names");
  }

  std::set<std::string> terminals;
  for (const Production &production : grammar.productions) {
    for (const std::string &symbol : production.rhs) {
      bool known_terminal = available_tokens.find(symbol) != available_tokens.end();
      bool known_nonterminal = nonterminals.find(symbol) != nonterminals.end();
      if (known_terminal && known_nonterminal) {
        throw ParserError("symbol cannot be both token and nonterminal: " +
                          symbol);
      }
      if (!known_terminal && !known_nonterminal) {
        throw ParserError("grammar symbol is neither token nor nonterminal: " +
                          symbol);
      }
      if (known_terminal) {
        terminals.insert(symbol);
      }
    }
  }
  return terminals;
}

void validateGrammarSymbols(const Grammar &grammar,
                            const std::set<std::string> &nonterminals,
                            const std::set<std::string> &terminals,
                            const std::set<std::string> &available_tokens) {
  if (nonterminals.find(grammar.start_symbol) == nonterminals.end()) {
    throw ParserError("grammar start symbol has no production: " +
                      grammar.start_symbol);
  }

  for (const std::string &terminal : available_tokens) {
    if (!isTerminalName(terminal)) {
      throw ParserError("token name must use TOKEN_CASE: " + terminal);
    }
  }

  for (const std::string &nonterminal : nonterminals) {
    if (isTerminalName(nonterminal)) {
      throw ParserError("nonterminal must not use TOKEN_CASE: " + nonterminal);
    }
  }

  for (const std::string &terminal : terminals) {
    if (nonterminals.find(terminal) != nonterminals.end()) {
      throw ParserError("symbol cannot be both terminal and nonterminal: " +
                        terminal);
    }
  }
}

bool addToSet(SymbolSet &target, const SymbolSet &source) {
  bool changed = false;
  for (const std::string &symbol : source) {
    changed = target.insert(symbol).second || changed;
  }
  return changed;
}

std::map<std::string, SymbolSet>
computeFirstSets(const Grammar &grammar,
                 const std::set<std::string> &terminals,
                 const std::set<std::string> &nonterminals) {
  std::map<std::string, SymbolSet> first;
  for (const std::string &terminal : terminals) {
    first[terminal].insert(terminal);
  }
  for (const std::string &nonterminal : nonterminals) {
    first[nonterminal];
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (const Production &production : grammar.productions) {
      if (production.rhs.empty()) {
        changed = first[production.lhs].insert(kEpsilon).second || changed;
        continue;
      }

      bool all_nullable = true;
      for (const std::string &symbol : production.rhs) {
        for (const std::string &first_symbol : first[symbol]) {
          if (first_symbol != kEpsilon) {
            changed =
                first[production.lhs].insert(first_symbol).second || changed;
          }
        }
        if (first[symbol].find(kEpsilon) == first[symbol].end()) {
          all_nullable = false;
          break;
        }
      }
      if (all_nullable) {
        changed = first[production.lhs].insert(kEpsilon).second || changed;
      }
    }
  }

  return first;
}

SymbolSet firstOfSequence(const std::vector<std::string> &symbols,
                          const std::map<std::string, SymbolSet> &first) {
  SymbolSet result;
  if (symbols.empty()) {
    result.insert(kEpsilon);
    return result;
  }

  bool all_nullable = true;
  for (const std::string &symbol : symbols) {
    auto found = first.find(symbol);
    if (found == first.end()) {
      throw ParserError("missing FIRST set for symbol: " + symbol);
    }
    for (const std::string &first_symbol : found->second) {
      if (first_symbol != kEpsilon) {
        result.insert(first_symbol);
      }
    }
    if (found->second.find(kEpsilon) == found->second.end()) {
      all_nullable = false;
      break;
    }
  }
  if (all_nullable) {
    result.insert(kEpsilon);
  }
  return result;
}

std::map<std::string, SymbolSet>
computeFollowSets(const Grammar &grammar,
                  const std::set<std::string> &nonterminals,
                  const std::map<std::string, SymbolSet> &first) {
  std::map<std::string, SymbolSet> follow;
  for (const std::string &nonterminal : nonterminals) {
    follow[nonterminal];
  }
  follow[grammar.start_symbol].insert(kEndSymbol);

  bool changed = true;
  while (changed) {
    changed = false;
    for (const Production &production : grammar.productions) {
      for (size_t i = 0; i < production.rhs.size(); ++i) {
        const std::string &symbol = production.rhs[i];
        if (nonterminals.find(symbol) == nonterminals.end()) {
          continue;
        }

        std::vector<std::string> suffix(production.rhs.begin() + i + 1,
                                        production.rhs.end());
        SymbolSet suffix_first = firstOfSequence(suffix, first);
        for (const std::string &first_symbol : suffix_first) {
          if (first_symbol != kEpsilon) {
            changed = follow[symbol].insert(first_symbol).second || changed;
          }
        }
        if (suffix_first.find(kEpsilon) != suffix_first.end()) {
          changed = addToSet(follow[symbol], follow[production.lhs]) || changed;
        }
      }
    }
  }

  return follow;
}

void setParseTableEntry(
    std::map<std::string, std::map<std::string, std::vector<int>>> &table,
    const std::string &nonterminal, const std::string &lookahead,
    int production_id) {
  std::vector<int> &entries = table[nonterminal][lookahead];
  if (std::find(entries.begin(), entries.end(), production_id) ==
      entries.end()) {
    entries.push_back(production_id);
  }
}

} // namespace

const char *endSymbol() { return kEndSymbol; }

const char *epsilonSymbol() { return kEpsilon; }

ParserAnalysis analyzeGrammar(const Grammar &grammar,
                              const std::set<std::string> &available_tokens) {
  if (grammar.start_symbol.empty()) {
    throw ParserError("grammar start symbol cannot be empty");
  }
  if (grammar.productions.empty()) {
    throw ParserError("grammar must contain at least one production");
  }

  ParserAnalysis analysis;
  analysis.productions = grammar.productions;
  analysis.nonterminals = collectNonterminals(grammar.productions);
  analysis.terminals =
      inferTerminals(grammar, analysis.nonterminals, available_tokens);

  validateGrammarSymbols(grammar, analysis.nonterminals, analysis.terminals,
                         available_tokens);

  std::map<std::string, SymbolSet> first =
      computeFirstSets(grammar, analysis.terminals, analysis.nonterminals);
  std::map<std::string, SymbolSet> follow =
      computeFollowSets(grammar, analysis.nonterminals, first);

  for (size_t production_id = 0; production_id < analysis.productions.size();
       ++production_id) {
    const Production &production = analysis.productions[production_id];
    SymbolSet rhs_first = firstOfSequence(production.rhs, first);
    for (const std::string &terminal : rhs_first) {
      if (terminal != kEpsilon) {
        setParseTableEntry(analysis.parse_table, production.lhs, terminal,
                           static_cast<int>(production_id));
      }
    }
    if (rhs_first.find(kEpsilon) != rhs_first.end()) {
      for (const std::string &terminal : follow[production.lhs]) {
        setParseTableEntry(analysis.parse_table, production.lhs, terminal,
                           static_cast<int>(production_id));
      }
    }
  }

  return analysis;
}

} // namespace compiler::parser
