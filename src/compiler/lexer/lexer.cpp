#include "compiler/lexer/lexer.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace compiler::lexer {
namespace {

struct Fragment {
  int start = -1;
  int accept = -1;
};

CharSet makeCharSet(unsigned char ch) {
  CharSet chars;
  chars.set(ch);
  return chars;
}

CharSet makeRange(unsigned char first, unsigned char last) {
  if (first > last) {
    throw LexerError("invalid character range in regex");
  }
  CharSet chars;
  for (int ch = first; ch <= last; ++ch) {
    chars.set(static_cast<size_t>(ch));
  }
  return chars;
}

CharSet makeAnyCharSet() {
  CharSet chars;
  chars.set();
  chars.reset(static_cast<size_t>('\n'));
  return chars;
}

CharSet makeDigitSet() { return makeRange('0', '9'); }

CharSet makeWhitespaceSet() {
  CharSet chars;
  chars.set(static_cast<size_t>(' '));
  chars.set(static_cast<size_t>('\t'));
  chars.set(static_cast<size_t>('\n'));
  chars.set(static_cast<size_t>('\r'));
  return chars;
}

CharSet makeWordSet() {
  CharSet chars = makeRange('a', 'z');
  chars |= makeRange('A', 'Z');
  chars |= makeRange('0', '9');
  chars.set(static_cast<size_t>('_'));
  return chars;
}

int singleChar(const CharSet &chars) {
  int found = -1;
  for (size_t i = 0; i < chars.size(); ++i) {
    if (!chars.test(i)) {
      continue;
    }
    if (found != -1) {
      return -1;
    }
    found = static_cast<int>(i);
  }
  return found;
}

class RegexParser {
public:
  explicit RegexParser(std::string regex) : regex_(std::move(regex)) {}

  Nfa parse() {
    Fragment fragment = parseExpression();
    if (!atEnd()) {
      throw LexerError("unexpected regex character: " +
                       std::string(1, regex_[pos_]));
    }
    nfa_.setStartState(fragment.start);
    nfa_.addAcceptState(fragment.accept);
    return nfa_;
  }

private:
  bool atEnd() const { return pos_ >= regex_.size(); }

  char peek() const {
    if (atEnd()) {
      return '\0';
    }
    return regex_[pos_];
  }

  char consume() {
    if (atEnd()) {
      throw LexerError("unexpected end of regex");
    }
    return regex_[pos_++];
  }

  bool match(char ch) {
    if (peek() != ch) {
      return false;
    }
    ++pos_;
    return true;
  }

  Fragment epsilonFragment() {
    int start = nfa_.addState();
    int accept = nfa_.addState();
    nfa_.addEpsilonTransition(start, accept);
    return {start, accept};
  }

  Fragment charSetFragment(const CharSet &chars) {
    int start = nfa_.addState();
    int accept = nfa_.addState();
    nfa_.addTransition(start, accept, chars);
    return {start, accept};
  }

  Fragment parseExpression() {
    Fragment left = parseConcat();
    while (match('|')) {
      Fragment right = parseConcat();
      int start = nfa_.addState();
      int accept = nfa_.addState();
      nfa_.addEpsilonTransition(start, left.start);
      nfa_.addEpsilonTransition(start, right.start);
      nfa_.addEpsilonTransition(left.accept, accept);
      nfa_.addEpsilonTransition(right.accept, accept);
      left = {start, accept};
    }
    return left;
  }

  Fragment parseConcat() {
    std::vector<Fragment> parts;
    while (!atEnd() && peek() != ')' && peek() != '|') {
      parts.push_back(parseRepeat());
    }
    if (parts.empty()) {
      return epsilonFragment();
    }
    Fragment result = parts.front();
    for (size_t i = 1; i < parts.size(); ++i) {
      nfa_.addEpsilonTransition(result.accept, parts[i].start);
      result.accept = parts[i].accept;
    }
    return result;
  }

  Fragment parseRepeat() {
    Fragment fragment = parseAtom();
    bool keep_parsing = true;
    while (keep_parsing) {
      switch (peek()) {
      case '*':
        consume();
        fragment = star(fragment);
        break;
      case '+':
        consume();
        fragment = plus(fragment);
        break;
      case '?':
        consume();
        fragment = optional(fragment);
        break;
      default:
        keep_parsing = false;
        break;
      }
    }
    return fragment;
  }

  Fragment parseAtom() {
    if (match('(')) {
      Fragment fragment = parseExpression();
      if (!match(')')) {
        throw LexerError("missing ')' in regex");
      }
      return fragment;
    }
    if (match('[')) {
      return charSetFragment(parseCharClass());
    }
    if (match('.')) {
      return charSetFragment(makeAnyCharSet());
    }
    if (peek() == ')' || peek() == '|' || peek() == '*' || peek() == '+' ||
        peek() == '?') {
      throw LexerError("unexpected regex operator: " + std::string(1, peek()));
    }
    if (match('\\')) {
      return charSetFragment(parseEscapeSet());
    }
    return charSetFragment(makeCharSet(static_cast<unsigned char>(consume())));
  }

  Fragment star(const Fragment &fragment) {
    int start = nfa_.addState();
    int accept = nfa_.addState();
    nfa_.addEpsilonTransition(start, fragment.start);
    nfa_.addEpsilonTransition(start, accept);
    nfa_.addEpsilonTransition(fragment.accept, fragment.start);
    nfa_.addEpsilonTransition(fragment.accept, accept);
    return {start, accept};
  }

  Fragment plus(const Fragment &fragment) {
    int start = nfa_.addState();
    int accept = nfa_.addState();
    nfa_.addEpsilonTransition(start, fragment.start);
    nfa_.addEpsilonTransition(fragment.accept, fragment.start);
    nfa_.addEpsilonTransition(fragment.accept, accept);
    return {start, accept};
  }

  Fragment optional(const Fragment &fragment) {
    int start = nfa_.addState();
    int accept = nfa_.addState();
    nfa_.addEpsilonTransition(start, fragment.start);
    nfa_.addEpsilonTransition(start, accept);
    nfa_.addEpsilonTransition(fragment.accept, accept);
    return {start, accept};
  }

  CharSet parseEscapeSet() {
    char escaped = consume();
    switch (escaped) {
    case 'n':
      return makeCharSet('\n');
    case 'r':
      return makeCharSet('\r');
    case 't':
      return makeCharSet('\t');
    case 'd':
      return makeDigitSet();
    case 's':
      return makeWhitespaceSet();
    case 'w':
      return makeWordSet();
    default:
      return makeCharSet(static_cast<unsigned char>(escaped));
    }
  }

  CharSet parseCharClassAtom() {
    if (match('\\')) {
      return parseEscapeSet();
    }
    return makeCharSet(static_cast<unsigned char>(consume()));
  }

  CharSet parseCharClass() {
    bool negate = match('^');
    CharSet chars;
    bool has_char = false;
    while (!atEnd() && peek() != ']') {
      CharSet left = parseCharClassAtom();
      if (peek() == '-' && pos_ + 1 < regex_.size() && regex_[pos_ + 1] != ']') {
        consume();
        CharSet right = parseCharClassAtom();
        int range_start = singleChar(left);
        int range_end = singleChar(right);
        if (range_start == -1 || range_end == -1) {
          throw LexerError("character class range endpoints must be single characters");
        }
        chars |= makeRange(static_cast<unsigned char>(range_start),
                           static_cast<unsigned char>(range_end));
      } else {
        chars |= left;
      }
      has_char = true;
    }
    if (!match(']')) {
      throw LexerError("missing ']' in regex");
    }
    if (!has_char) {
      throw LexerError("empty character class in regex");
    }
    if (negate) {
      chars.flip();
    }
    return chars;
  }

  std::string regex_;
  size_t pos_ = 0;
  Nfa nfa_;
};

struct CombinedNfaState {
  std::vector<int> epsilon_transitions;
  std::vector<std::pair<CharSet, int>> transitions;
  std::vector<int> accepted_token_ids;
};

using StateSet = std::set<int>;

StateSet epsilonClosure(const std::vector<CombinedNfaState> &states,
                        const StateSet &seed) {
  StateSet closure = seed;
  std::queue<int> worklist;
  for (int state : seed) {
    worklist.push(state);
  }
  while (!worklist.empty()) {
    int state = worklist.front();
    worklist.pop();
    for (int next : states[state].epsilon_transitions) {
      if (closure.insert(next).second) {
        worklist.push(next);
      }
    }
  }
  return closure;
}

StateSet moveByChar(const std::vector<CombinedNfaState> &states,
                    const StateSet &from, unsigned char ch) {
  StateSet result;
  for (int state : from) {
    for (const auto &transition : states[state].transitions) {
      if (transition.first.test(static_cast<size_t>(ch))) {
        result.insert(transition.second);
      }
    }
  }
  return result;
}

std::vector<int> acceptedTokens(const std::vector<CombinedNfaState> &states,
                                const StateSet &dfa_state) {
  std::set<int> accepted;
  for (int state : dfa_state) {
    accepted.insert(states[state].accepted_token_ids.begin(),
                    states[state].accepted_token_ids.end());
  }
  return {accepted.begin(), accepted.end()};
}

void appendTokenNfa(std::vector<CombinedNfaState> &combined, const Nfa &nfa,
                    int token_id) {
  if (nfa.startState() == -1) {
    throw LexerError("token automaton has no start state");
  }
  int offset = static_cast<int>(combined.size());
  combined.resize(combined.size() + nfa.states().size());
  combined[0].epsilon_transitions.push_back(offset + nfa.startState());

  for (size_t i = 0; i < nfa.states().size(); ++i) {
    const auto &from = nfa.states()[i];
    auto &to = combined[offset + static_cast<int>(i)];
    for (int next : from.epsilon_transitions) {
      to.epsilon_transitions.push_back(offset + next);
    }
    for (const auto &transition : from.transitions) {
      to.transitions.push_back({transition.first, offset + transition.second});
    }
    if (from.accepting) {
      to.accepted_token_ids.push_back(token_id);
    }
  }
}

} // namespace

AmbiguousLexerError::AmbiguousLexerError(
    const std::vector<Ambiguity> &ambiguities)
    : LexerError([&]() {
        std::ostringstream message;
        message << "ambiguous lexer automaton";
        for (const auto &ambiguity : ambiguities) {
          message << "; DFA state " << ambiguity.state_id << " accepts ";
          for (size_t i = 0; i < ambiguity.token_names.size(); ++i) {
            if (i != 0) {
              message << ", ";
            }
            message << ambiguity.token_names[i];
          }
        }
        return message.str();
      }()) {}

int Nfa::addState() {
  states_.push_back(State{});
  return static_cast<int>(states_.size()) - 1;
}

void Nfa::setStartState(int state) {
  checkState(state);
  start_state_ = state;
}

void Nfa::addAcceptState(int state) {
  checkState(state);
  states_[state].accepting = true;
}

void Nfa::addTransition(int from, int to, unsigned char ch) {
  addTransition(from, to, makeCharSet(ch));
}

void Nfa::addTransition(int from, int to, const CharSet &chars) {
  checkState(from);
  checkState(to);
  states_[from].transitions.push_back({chars, to});
}

void Nfa::addEpsilonTransition(int from, int to) {
  checkState(from);
  checkState(to);
  states_[from].epsilon_transitions.push_back(to);
}

int Nfa::startState() const { return start_state_; }

const std::vector<Nfa::State> &Nfa::states() const { return states_; }

void Nfa::checkState(int state) const {
  if (state < 0 || state >= static_cast<int>(states_.size())) {
    throw LexerError("invalid NFA state id");
  }
}

TokenSpec TokenSpec::fromRegex(std::string name, std::string regex,
                               bool skipped, int priority) {
  TokenSpec token;
  token.name = std::move(name);
  token.kind = Kind::Regex;
  token.regex = std::move(regex);
  token.skipped = skipped;
  token.priority = priority;
  return token;
}

TokenSpec TokenSpec::fromAutomaton(std::string name, Nfa automaton,
                                   bool skipped, int priority) {
  TokenSpec token;
  token.name = std::move(name);
  token.kind = Kind::Automaton;
  token.automaton = std::move(automaton);
  token.skipped = skipped;
  token.priority = priority;
  return token;
}

std::vector<Token> Lexer::tokenize(std::istream &input) const {
  ensureUnambiguous();

  std::string text((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
  std::vector<Token> tokens;
  size_t pos = 0;

  while (pos < text.size()) {
    int state = 0;
    int last_accept_state = -1;
    size_t last_accept_pos = pos;
    size_t cursor = pos;

    while (cursor < text.size()) {
      unsigned char ch = static_cast<unsigned char>(text[cursor]);
      int next = states_[state].transitions[ch];
      if (next == -1) {
        break;
      }
      state = next;
      ++cursor;
      if (!states_[state].accepted_token_ids.empty()) {
        last_accept_state = state;
        last_accept_pos = cursor;
      }
    }

    if (last_accept_state == -1) {
      std::ostringstream message;
      message << "unrecognized character at offset " << pos << ": '"
              << text[pos] << "'";
      throw LexerError(message.str());
    }

    int token_id = states_[last_accept_state].accepted_token_ids.front();
    if (!token_skipped_[token_id]) {
      std::string lexeme = text.substr(pos, last_accept_pos - pos);
      if (token_names_[token_id].find("INVALID_") == 0) {
        throw LexerError("invalid token '" + lexeme + "' (" +
                         token_names_[token_id] + ")");
      }
      tokens.push_back({token_names_[token_id], lexeme});
    }
    pos = last_accept_pos;
  }

  return tokens;
}

bool Lexer::hasAmbiguity() const { return !ambiguities_.empty(); }

const std::vector<Ambiguity> &Lexer::ambiguities() const {
  return ambiguities_;
}

void Lexer::ensureUnambiguous() const {
  if (hasAmbiguity()) {
    throw AmbiguousLexerError(ambiguities_);
  }
}

void Lexer::save(std::ostream &output) const {
  output << "tokens " << token_names_.size() << '\n';
  for (size_t i = 0; i < token_names_.size(); ++i) {
    output << i << ' ' << token_names_[i] << '\n';
  }

  output << "states " << states_.size() << '\n';
  for (size_t state_id = 0; state_id < states_.size(); ++state_id) {
    const auto &state = states_[state_id];
    output << "state " << state_id;
    if (!state.accepted_token_ids.empty()) {
      output << " accepts";
      for (int token_id : state.accepted_token_ids) {
        output << ' ' << token_names_[token_id];
        if (token_skipped_[token_id]) {
          output << "(skip)";
        }
      }
    }
    output << '\n';
    for (int ch = 0; ch < 256; ++ch) {
      if (state.transitions[ch] != -1) {
        output << "  " << ch << " -> " << state.transitions[ch] << '\n';
      }
    }
  }
}

void Lexer::save(const std::string &path) const {
  std::ofstream output(path);
  if (!output) {
    throw LexerError("cannot open lexer automaton output file: " + path);
  }
  save(output);
}

LexerBuilder &LexerBuilder::addToken(const TokenSpec &token) {
  if (token.name.empty()) {
    throw LexerError("token name cannot be empty");
  }
  TokenSpec ordered_token = token;
  if (ordered_token.priority < 0) {
    ordered_token.priority = static_cast<int>(tokens_.size());
  }
  tokens_.push_back(std::move(ordered_token));
  return *this;
}

LexerBuilder &LexerBuilder::addTokenRegex(std::string name, std::string regex) {
  return addToken(TokenSpec::fromRegex(std::move(name), std::move(regex)));
}

LexerBuilder &LexerBuilder::addTokenAutomaton(std::string name, Nfa automaton) {
  return addToken(
      TokenSpec::fromAutomaton(std::move(name), std::move(automaton)));
}

LexerBuilder &LexerBuilder::skipRegex(std::string name, std::string regex) {
  return addToken(
      TokenSpec::fromRegex(std::move(name), std::move(regex), true));
}

LexerBuilder &LexerBuilder::skipAutomaton(std::string name, Nfa automaton) {
  return addToken(
      TokenSpec::fromAutomaton(std::move(name), std::move(automaton), true));
}

Lexer LexerBuilder::build() const {
  if (tokens_.empty()) {
    throw LexerError("cannot build lexer without token definitions");
  }

  std::vector<CombinedNfaState> combined(1);
  Lexer lexer;
  for (size_t token_id = 0; token_id < tokens_.size(); ++token_id) {
    const auto &token = tokens_[token_id];
    lexer.token_names_.push_back(token.name);
    lexer.token_skipped_.push_back(token.skipped);
    lexer.token_priorities_.push_back(token.priority);
    Nfa nfa = token.kind == TokenSpec::Kind::Regex
                  ? RegexParser(token.regex).parse()
                  : token.automaton;
    appendTokenNfa(combined, nfa, static_cast<int>(token_id));
  }

  std::map<StateSet, int> dfa_state_ids;
  std::queue<StateSet> worklist;

  StateSet start_seed = {0};
  StateSet start = epsilonClosure(combined, start_seed);
  dfa_state_ids[start] = 0;
  lexer.states_.push_back(Lexer::DfaState{});
  lexer.states_[0].transitions.fill(-1);
  lexer.states_[0].accepted_token_ids = acceptedTokens(combined, start);
  worklist.push(start);

  while (!worklist.empty()) {
    StateSet current = worklist.front();
    worklist.pop();
    int current_id = dfa_state_ids[current];

    for (int ch = 0; ch < 256; ++ch) {
      StateSet moved =
          moveByChar(combined, current, static_cast<unsigned char>(ch));
      if (moved.empty()) {
        continue;
      }
      StateSet next = epsilonClosure(combined, moved);
      auto [it, inserted] =
          dfa_state_ids.insert({next, static_cast<int>(lexer.states_.size())});
      if (inserted) {
        lexer.states_.push_back(Lexer::DfaState{});
        lexer.states_.back().transitions.fill(-1);
        lexer.states_.back().accepted_token_ids = acceptedTokens(combined, next);
        worklist.push(next);
      }
      lexer.states_[current_id].transitions[ch] = it->second;
    }
  }

  for (size_t state_id = 0; state_id < lexer.states_.size(); ++state_id) {
    auto &accepted = lexer.states_[state_id].accepted_token_ids;
    if (accepted.size() <= 1) {
      continue;
    }
    std::sort(accepted.begin(), accepted.end(), [&](int left, int right) {
      return lexer.token_priorities_[left] < lexer.token_priorities_[right];
    });
    int best_priority = lexer.token_priorities_[accepted.front()];
    std::vector<int> best_tokens;
    for (int token_id : accepted) {
      if (lexer.token_priorities_[token_id] == best_priority) {
        best_tokens.push_back(token_id);
      }
    }
    if (best_tokens.size() <= 1) {
      continue;
    }
    Ambiguity ambiguity;
    ambiguity.state_id = static_cast<int>(state_id);
    for (int token_id : best_tokens) {
      ambiguity.token_names.push_back(lexer.token_names_[token_id]);
    }
    lexer.ambiguities_.push_back(std::move(ambiguity));
  }

  return lexer;
}

} // namespace compiler::lexer
