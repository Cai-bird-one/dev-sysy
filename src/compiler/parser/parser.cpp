#include "compiler/parser/parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

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

std::string lookaheadName(const std::vector<compiler::lexer::Token> &tokens,
                          size_t index) {
  if (index >= tokens.size()) {
    return kEndSymbol;
  }
  return tokens[index].name;
}

std::string lookaheadLexeme(const std::vector<compiler::lexer::Token> &tokens,
                            size_t index) {
  if (index >= tokens.size()) {
    return kEndSymbol;
  }
  return tokens[index].lexeme;
}

void setParseTableEntry(std::map<std::string, std::map<std::string, int>> &table,
                        const std::string &nonterminal,
                        const std::string &lookahead, int production_id) {
  auto found = table[nonterminal].find(lookahead);
  if (found != table[nonterminal].end() && found->second != production_id) {
    std::ostringstream message;
    message << "LL(1) conflict for " << nonterminal << " on " << lookahead
            << ": production " << found->second << " vs " << production_id;
    throw ParserConflictError(message.str());
  }
  table[nonterminal][lookahead] = production_id;
}

} // namespace

ParseNode::ParseNode(std::string symbol) : symbol(std::move(symbol)) {}

ParseNode::ParseNode(std::string symbol, std::string lexeme)
    : symbol(std::move(symbol)), lexeme(std::move(lexeme)) {}

std::unique_ptr<ParseNode>
Parser::parse(const std::vector<compiler::lexer::Token> &tokens) const {
  size_t input_pos = 0;
  std::unique_ptr<ParseNode> tree =
      parseSymbol(grammar_.start_symbol, tokens, input_pos);
  if (input_pos != tokens.size()) {
    throw ParserError("unexpected trailing token '" +
                      lookaheadLexeme(tokens, input_pos) + "' (" +
                      lookaheadName(tokens, input_pos) + ")");
  }
  return tree;
}

std::unique_ptr<ParseNode>
Parser::parseSymbol(const std::string &symbol,
                    const std::vector<compiler::lexer::Token> &tokens,
                    size_t &input_pos) const {
  if (terminals_.find(symbol) != terminals_.end()) {
    std::string lookahead = lookaheadName(tokens, input_pos);
    if (lookahead != symbol) {
      throw ParserError("expected " + symbol + ", got '" +
                        lookaheadLexeme(tokens, input_pos) + "' (" +
                        lookahead + ")");
    }
    auto node =
        std::make_unique<ParseNode>(tokens[input_pos].name, tokens[input_pos].lexeme);
    ++input_pos;
    return node;
  }

  auto nonterminal_found = parse_table_.find(symbol);
  if (nonterminal_found == parse_table_.end()) {
    throw ParserError("unknown parser symbol: " + symbol);
  }

  std::string lookahead = lookaheadName(tokens, input_pos);
  auto production_found = nonterminal_found->second.find(lookahead);
  if (production_found == nonterminal_found->second.end()) {
    throw ParserError("unexpected token '" + lookaheadLexeme(tokens, input_pos) +
                      "' (" + lookahead + ") while parsing " + symbol);
  }

  const Production &production = productions_[production_found->second];
  auto node = std::make_unique<ParseNode>(symbol);
  for (const std::string &child_symbol : production.rhs) {
    node->children.push_back(parseSymbol(child_symbol, tokens, input_pos));
  }
  return node;
}

void Parser::save(std::ostream &output) const {
  output << "productions " << productions_.size() << '\n';
  for (size_t i = 0; i < productions_.size(); ++i) {
    output << i << ": " << productions_[i].lhs << " ->";
    if (productions_[i].rhs.empty()) {
      output << ' ' << kEpsilon;
    } else {
      for (const std::string &symbol : productions_[i].rhs) {
        output << ' ' << symbol;
      }
    }
    output << '\n';
  }

  output << "ll1_table\n";
  for (const auto &row : parse_table_) {
    for (const auto &entry : row.second) {
      output << "  " << row.first << ", " << entry.first << " -> "
             << entry.second << '\n';
    }
  }
}

void Parser::save(const std::string &path) const {
  std::ofstream output(path);
  if (!output) {
    throw ParserError("cannot open parser table output file: " + path);
  }
  save(output);
}

ParserBuilder::ParserBuilder(Grammar grammar) : grammar_(std::move(grammar)) {}

ParserBuilder &
ParserBuilder::setAvailableTokens(std::set<std::string> token_names) {
  available_token_names_ = std::move(token_names);
  return *this;
}

Parser ParserBuilder::build() const {
  if (grammar_.start_symbol.empty()) {
    throw ParserError("grammar start symbol cannot be empty");
  }
  if (grammar_.productions.empty()) {
    throw ParserError("grammar must contain at least one production");
  }

  Parser parser;
  parser.grammar_ = grammar_;
  parser.productions_ = grammar_.productions;
  parser.nonterminals_ = collectNonterminals(grammar_.productions);
  parser.terminals_ =
      inferTerminals(grammar_, parser.nonterminals_, available_token_names_);

  validateGrammarSymbols(grammar_, parser.nonterminals_, parser.terminals_,
                         available_token_names_);

  std::map<std::string, SymbolSet> first =
      computeFirstSets(grammar_, parser.terminals_, parser.nonterminals_);
  std::map<std::string, SymbolSet> follow =
      computeFollowSets(grammar_, parser.nonterminals_, first);

  for (size_t production_id = 0; production_id < parser.productions_.size();
       ++production_id) {
    const Production &production = parser.productions_[production_id];
    SymbolSet rhs_first = firstOfSequence(production.rhs, first);
    for (const std::string &terminal : rhs_first) {
      if (terminal != kEpsilon) {
        setParseTableEntry(parser.parse_table_, production.lhs, terminal,
                           static_cast<int>(production_id));
      }
    }
    if (rhs_first.find(kEpsilon) != rhs_first.end()) {
      for (const std::string &terminal : follow[production.lhs]) {
        setParseTableEntry(parser.parse_table_, production.lhs, terminal,
                           static_cast<int>(production_id));
      }
    }
  }

  return parser;
}

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
