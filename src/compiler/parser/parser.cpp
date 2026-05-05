#include "compiler/parser/parser.h"
#include "compiler/parser/parser_analysis.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace compiler::parser {
namespace {

std::string lookaheadName(const std::vector<compiler::lexer::Token> &tokens,
                          size_t index) {
  if (index >= tokens.size()) {
    return endSymbol();
  }
  return tokens[index].name;
}

std::string lookaheadLexeme(const std::vector<compiler::lexer::Token> &tokens,
                            size_t index) {
  if (index >= tokens.size()) {
    return endSymbol();
  }
  return tokens[index].lexeme;
}

} // namespace

std::unique_ptr<ParseNode>
Parser::parse(const std::vector<compiler::lexer::Token> &tokens) const {
  size_t input_pos = 0;
  farthest_error_pos_ = 0;
  farthest_error_message_.clear();
  std::unique_ptr<ParseNode> tree;
  try {
    tree = parseSymbol(grammar_.start_symbol, tokens, input_pos);
  } catch (const ParserError &error) {
    if (!farthest_error_message_.empty()) {
      throw ParserError(farthest_error_message_);
    }
    throw;
  }
  if (input_pos != tokens.size()) {
    std::string message = "unexpected trailing token '" +
                          lookaheadLexeme(tokens, input_pos) + "' (" +
                          lookaheadName(tokens, input_pos) + ")";
    rememberError(input_pos, message);
    throw ParserError(message);
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
      std::string message = "expected " + symbol + ", got '" +
                            lookaheadLexeme(tokens, input_pos) + "' (" +
                            lookahead + ")";
      rememberError(input_pos, message);
      throw ParserError(message);
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
    std::string message = "unexpected token '" +
                          lookaheadLexeme(tokens, input_pos) + "' (" +
                          lookahead + ") while parsing " + symbol;
    rememberError(input_pos, message);
    throw ParserError(message);
  }

  ParserError last_error("no production matched while parsing " + symbol);
  size_t farthest_error_pos = input_pos;
  for (int production_id : production_found->second) {
    const Production &production = productions_[production_id];
    auto node = std::make_unique<ParseNode>(symbol);
    size_t trial_pos = input_pos;
    try {
      for (const std::string &child_symbol : production.rhs) {
        node->children.push_back(parseSymbol(child_symbol, tokens, trial_pos));
      }
      input_pos = trial_pos;
      return node;
    } catch (const ParserError &error) {
      if (trial_pos >= farthest_error_pos) {
        farthest_error_pos = trial_pos;
        last_error = error;
      }
    }
  }

  throw last_error;
}

void Parser::rememberError(size_t input_pos,
                           const std::string &message) const {
  if (farthest_error_message_.empty() || input_pos >= farthest_error_pos_) {
    farthest_error_pos_ = input_pos;
    farthest_error_message_ = message;
  }
}

void Parser::save(std::ostream &output) const {
  output << "productions " << productions_.size() << '\n';
  for (size_t i = 0; i < productions_.size(); ++i) {
    output << i << ": " << productions_[i].lhs << " ->";
    if (productions_[i].rhs.empty()) {
      output << ' ' << epsilonSymbol();
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
      output << "  " << row.first << ", " << entry.first << " ->";
      for (int production_id : entry.second) {
        output << ' ' << production_id;
      }
      output << '\n';
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
  ParserAnalysis analysis = analyzeGrammar(grammar_, available_token_names_);

  Parser parser;
  parser.grammar_ = grammar_;
  parser.productions_ = std::move(analysis.productions);
  parser.nonterminals_ = std::move(analysis.nonterminals);
  parser.terminals_ = std::move(analysis.terminals);
  parser.parse_table_ = std::move(analysis.parse_table);

  return parser;
}

} // namespace compiler::parser
