#pragma once

#include "compiler/lexer/lexer.h"

#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace compiler::parser {

struct Production {
  std::string lhs;
  std::vector<std::string> rhs;
};

struct Grammar {
  std::string start_symbol;
  std::vector<Production> productions;
};

struct ParseNode {
  std::string symbol;
  std::string lexeme;
  std::vector<std::unique_ptr<ParseNode>> children;

  explicit ParseNode(std::string symbol);
  ParseNode(std::string symbol, std::string lexeme);
};

class ParserError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class ParserConflictError : public ParserError {
public:
  using ParserError::ParserError;
};

class Parser {
public:
  std::unique_ptr<ParseNode>
  parse(const std::vector<compiler::lexer::Token> &tokens) const;

  void save(std::ostream &output) const;
  void save(const std::string &path) const;

private:
  friend class ParserBuilder;

  std::unique_ptr<ParseNode>
  parseSymbol(const std::string &symbol,
              const std::vector<compiler::lexer::Token> &tokens,
              size_t &input_pos) const;

  Grammar grammar_;
  std::vector<Production> productions_;
  std::set<std::string> terminals_;
  std::set<std::string> nonterminals_;
  std::map<std::string, std::map<std::string, std::vector<int>>> parse_table_;
};

class ParserBuilder {
public:
  explicit ParserBuilder(Grammar grammar);

  ParserBuilder &setAvailableTokens(std::set<std::string> token_names);
  Parser build() const;

private:
  Grammar grammar_;
  std::set<std::string> available_token_names_;
};

void printParseTree(const ParseNode &node, std::ostream &output,
                    int indent = 0);

} // namespace compiler::parser
