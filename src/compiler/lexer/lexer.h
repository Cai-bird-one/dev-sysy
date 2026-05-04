#pragma once

#include <array>
#include <bitset>
#include <iosfwd>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace compiler::lexer {

using CharSet = std::bitset<256>;

struct Token {
  std::string name;
  std::string lexeme;
};

struct Ambiguity {
  int state_id = -1;
  std::vector<std::string> token_names;
};

class LexerError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class AmbiguousLexerError : public LexerError {
public:
  explicit AmbiguousLexerError(const std::vector<Ambiguity> &ambiguities);
};

class Nfa {
public:
  struct State {
    std::vector<int> epsilon_transitions;
    std::vector<std::pair<CharSet, int>> transitions;
    bool accepting = false;
  };

  int addState();
  void setStartState(int state);
  void addAcceptState(int state);
  void addTransition(int from, int to, unsigned char ch);
  void addTransition(int from, int to, const CharSet &chars);
  void addEpsilonTransition(int from, int to);

  int startState() const;
  const std::vector<State> &states() const;

private:
  void checkState(int state) const;

  int start_state_ = -1;
  std::vector<State> states_;
};

struct TokenSpec {
  enum class Kind {
    Regex,
    Automaton,
  };

  std::string name;
  Kind kind = Kind::Regex;
  std::string regex;
  Nfa automaton;
  bool skipped = false;
  int priority = -1;

  static TokenSpec fromRegex(std::string name, std::string regex,
                             bool skipped = false, int priority = -1);
  static TokenSpec fromAutomaton(std::string name, Nfa automaton,
                                 bool skipped = false, int priority = -1);
};

class Lexer {
public:
  std::vector<Token> tokenize(std::istream &input) const;

  bool hasAmbiguity() const;
  const std::vector<Ambiguity> &ambiguities() const;
  void ensureUnambiguous() const;

  void save(std::ostream &output) const;
  void save(const std::string &path) const;

private:
  friend class LexerBuilder;
  friend Lexer buildLexerAutomaton(const std::vector<TokenSpec> &tokens);

  struct DfaState {
    std::array<int, 256> transitions{};
    std::vector<int> accepted_token_ids;
  };

  std::vector<std::string> token_names_;
  std::vector<bool> token_skipped_;
  std::vector<int> token_priorities_;
  std::vector<DfaState> states_;
  std::vector<Ambiguity> ambiguities_;
};

class LexerBuilder {
public:
  LexerBuilder &addToken(const TokenSpec &token);
  LexerBuilder &addTokenRegex(std::string name, std::string regex);
  LexerBuilder &addTokenAutomaton(std::string name, Nfa automaton);
  LexerBuilder &skipRegex(std::string name, std::string regex);
  LexerBuilder &skipAutomaton(std::string name, Nfa automaton);

  Lexer build() const;

private:
  std::vector<TokenSpec> tokens_;
};

} // namespace compiler::lexer
