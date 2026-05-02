#include "compiler/parser/grammar_rules.h"

#include "compiler/lexer/token_rules.h"

namespace compiler::parser {
namespace {

const Grammar kDefaultGrammar = {
    "CompUnit",
    {
        {"CompUnit", {"FuncDef"}},
        {"FuncDef",
         {"FuncType", "IDENT", "LPAREN", "RPAREN", "Block"}},
        {"FuncType", {"INT"}},
        {"Block", {"LBRACE", "Stmt", "RBRACE"}},
        {"Stmt", {"RETURN", "Number", "SEMICOLON"}},
        {"Number", {"INT_CONST"}},
    },
};

} // namespace

const Grammar &defaultGrammar() { return kDefaultGrammar; }

ParserBuilder defaultParserBuilder() {
  return ParserBuilder(defaultGrammar())
      .setAvailableTokens(compiler::lexer::defaultTokenNames());
}

Parser buildDefaultParser() { return defaultParserBuilder().build(); }

} // namespace compiler::parser
