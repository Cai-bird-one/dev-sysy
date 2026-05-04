#include "compiler/lexer/automaton.h"
#include "compiler/lexer/regex.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <utility>

namespace compiler::lexer {
namespace {

CharSet makeCharSet(unsigned char ch) {
  CharSet chars;
  chars.set(ch);
  return chars;
}

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

Lexer buildLexerAutomaton(const std::vector<TokenSpec> &tokens) {
  if (tokens.empty()) {
    throw LexerError("cannot build lexer without token definitions");
  }

  std::vector<CombinedNfaState> combined(1);
  Lexer lexer;
  for (size_t token_id = 0; token_id < tokens.size(); ++token_id) {
    const auto &token = tokens[token_id];
    lexer.token_names_.push_back(token.name);
    lexer.token_skipped_.push_back(token.skipped);
    lexer.token_priorities_.push_back(token.priority);
    Nfa nfa = token.kind == TokenSpec::Kind::Regex ? regexToNfa(token.regex)
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
