#include "compiler/lexer/regex.h"

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
  chars.set(static_cast<size_t>('\v'));
  chars.set(static_cast<size_t>('\f'));
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
    case 'v':
      return makeCharSet('\v');
    case 'f':
      return makeCharSet('\f');
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
          throw LexerError(
              "character class range endpoints must be single characters");
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

} // namespace

Nfa regexToNfa(std::string regex) {
  return RegexParser(std::move(regex)).parse();
}

} // namespace compiler::lexer
