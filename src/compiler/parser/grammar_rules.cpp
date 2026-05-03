#include "compiler/parser/grammar_rules.h"

#include "compiler/lexer/token_rules.h"

namespace compiler::parser {
namespace {

const Grammar kDefaultGrammar = {
    "CompUnit",
    {
        {"CompUnit", {"TopItem", "TopItems"}},
        {"TopItems", {"TopItem", "TopItems"}},
        {"TopItems", {}},
        {"TopItem", {"CONST", "BType", "ConstDef", "ConstDefList", "SEMICOLON"}},
        {"TopItem", {"INT", "IDENT", "IntTopTail"}},
        {"IntTopTail", {"LPAREN", "RPAREN", "Block"}},
        {"IntTopTail", {"VarDefAfterIdent", "VarDefList", "SEMICOLON"}},
        {"VarDefAfterIdent", {"ASSIGN", "InitVal"}},
        {"VarDefAfterIdent", {}},
        {"FuncDef",
         {"FuncType", "IDENT", "LPAREN", "RPAREN", "Block"}},
        {"FuncType", {"INT"}},
        {"Block", {"LBRACE", "BlockItems", "RBRACE"}},
        {"BlockItems", {"BlockItem", "BlockItems"}},
        {"BlockItems", {}},
        {"BlockItem", {"Decl"}},
        {"BlockItem", {"Stmt"}},
        {"Decl", {"ConstDecl"}},
        {"Decl", {"VarDecl"}},
        {"ConstDecl",
         {"CONST", "BType", "ConstDef", "ConstDefList", "SEMICOLON"}},
        {"BType", {"INT"}},
        {"ConstDefList", {"COMMA", "ConstDef", "ConstDefList"}},
        {"ConstDefList", {}},
        {"ConstDef", {"IDENT", "ASSIGN", "ConstInitVal"}},
        {"ConstInitVal", {"ConstExp"}},
        {"ConstExp", {"Exp"}},
        {"VarDecl", {"BType", "VarDef", "VarDefList", "SEMICOLON"}},
        {"VarDefList", {"COMMA", "VarDef", "VarDefList"}},
        {"VarDefList", {}},
        {"VarDef", {"IDENT", "VarDefTail"}},
        {"VarDefTail", {"ASSIGN", "InitVal"}},
        {"VarDefTail", {}},
        {"InitVal", {"Exp"}},
        {"Stmt", {"RETURN", "Exp", "SEMICOLON"}},
        {"Exp", {"AddExp"}},
        {"AddExp", {"MulExp", "AddExpTail"}},
        {"AddExpTail", {"PLUS", "MulExp", "AddExpTail"}},
        {"AddExpTail", {"MINUS", "MulExp", "AddExpTail"}},
        {"AddExpTail", {}},
        {"MulExp", {"UnaryExp", "MulExpTail"}},
        {"MulExpTail", {"STAR", "UnaryExp", "MulExpTail"}},
        {"MulExpTail", {"SLASH", "UnaryExp", "MulExpTail"}},
        {"MulExpTail", {"PERCENT", "UnaryExp", "MulExpTail"}},
        {"MulExpTail", {}},
        {"UnaryExp", {"PrimaryExp"}},
        {"UnaryExp", {"UnaryOp", "UnaryExp"}},
        {"PrimaryExp", {"LPAREN", "Exp", "RPAREN"}},
        {"PrimaryExp", {"LVal"}},
        {"PrimaryExp", {"Number"}},
        {"LVal", {"IDENT"}},
        {"UnaryOp", {"PLUS"}},
        {"UnaryOp", {"MINUS"}},
        {"UnaryOp", {"NOT"}},
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
