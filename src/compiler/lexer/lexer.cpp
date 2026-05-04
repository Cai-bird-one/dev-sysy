#include "compiler/lexer/lexer.h"
#include "compiler/lexer/automaton.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>

namespace compiler::lexer {

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
  return buildLexerAutomaton(tokens_);
}

} // namespace compiler::lexer
