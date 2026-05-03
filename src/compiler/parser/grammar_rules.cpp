#include "compiler/parser/grammar_rules.h"

#include "compiler/lexer/token_rules.h"

namespace compiler::parser {
namespace {

const Grammar kDefaultGrammar = {
    "CompUnit",
    {
        {"CompUnit", {"TopItem", "CompUnitTail"}},
        {"CompUnitTail", {"TopItem", "CompUnitTail"}},
        {"CompUnitTail", {}},

        {"TopItem", {"FuncDef"}},
        {"TopItem", {"Decl"}},
        {"Decl", {"ConstDecl"}},
        {"Decl", {"VarDecl"}},

        {"ConstDecl",
         {"CONST", "BType", "ConstDef", "ConstDefList", "SEMICOLON"}},
        {"BType", {"INT"}},
        {"ConstDefList", {"COMMA", "ConstDef", "ConstDefList"}},
        {"ConstDefList", {}},
        {"ConstDef", {"IDENT", "ConstArrayDims", "ASSIGN", "ConstInitVal"}},
        {"ConstArrayDims", {"LBRACKET", "ConstExp", "RBRACKET",
                            "ConstArrayDims"}},
        {"ConstArrayDims", {}},
        {"ConstInitVal", {"ConstExp"}},
        {"ConstInitVal", {"LBRACE", "ConstInitValListOpt", "RBRACE"}},
        {"ConstInitValListOpt",
         {"ConstInitVal", "ConstInitValListTail"}},
        {"ConstInitValListOpt", {}},
        {"ConstInitValListTail",
         {"COMMA", "ConstInitVal", "ConstInitValListTail"}},
        {"ConstInitValListTail", {"COMMA"}},
        {"ConstInitValListTail", {}},
        {"ConstExp", {"Exp"}},

        {"VarDecl", {"BType", "VarDef", "VarDefList", "SEMICOLON"}},
        {"VarDefList", {"COMMA", "VarDef", "VarDefList"}},
        {"VarDefList", {}},
        {"VarDef", {"IDENT", "ConstArrayDims", "VarDefInitOpt"}},
        {"VarDefInitOpt", {"ASSIGN", "InitVal"}},
        {"VarDefInitOpt", {}},
        {"InitVal", {"Exp"}},
        {"InitVal", {"LBRACE", "InitValListOpt", "RBRACE"}},
        {"InitValListOpt", {"InitVal", "InitValListTail"}},
        {"InitValListOpt", {}},
        {"InitValListTail", {"COMMA", "InitVal", "InitValListTail"}},
        {"InitValListTail", {"COMMA"}},
        {"InitValListTail", {}},

        {"FuncDef",
         {"FuncType", "IDENT", "LPAREN", "FuncFParamsOpt", "RPAREN",
          "Block"}},
        {"FuncType", {"VOID"}},
        {"FuncType", {"INT"}},
        {"FuncFParamsOpt", {"VOID"}},
        {"FuncFParamsOpt", {"FuncFParams"}},
        {"FuncFParamsOpt", {}},
        {"FuncFParams", {"FuncFParam", "FuncFParamsTail"}},
        {"FuncFParamsTail", {"COMMA", "FuncFParam", "FuncFParamsTail"}},
        {"FuncFParamsTail", {}},
        {"FuncFParam", {"BType", "IDENT", "FuncFParamArrayOpt"}},
        {"FuncFParamArrayOpt",
         {"LBRACKET", "RBRACKET", "FuncFParamArrayDims"}},
        {"FuncFParamArrayOpt", {}},
        {"FuncFParamArrayDims",
         {"LBRACKET", "ConstExp", "RBRACKET", "FuncFParamArrayDims"}},
        {"FuncFParamArrayDims", {}},

        {"Block", {"LBRACE", "BlockItems", "RBRACE"}},
        {"BlockItems", {"BlockItem", "BlockItems"}},
        {"BlockItems", {}},
        {"BlockItem", {"Decl"}},
        {"BlockItem", {"Stmt"}},

        {"Stmt", {"LVal", "ASSIGN", "Exp", "SEMICOLON"}},
        {"Stmt", {"Exp", "SEMICOLON"}},
        {"Stmt", {"SEMICOLON"}},
        {"Stmt", {"Block"}},
        {"Stmt", {"IF", "LPAREN", "Exp", "RPAREN", "Stmt", "ElseOpt"}},
        {"Stmt", {"WHILE", "LPAREN", "Exp", "RPAREN", "Stmt"}},
        {"Stmt", {"BREAK", "SEMICOLON"}},
        {"Stmt", {"CONTINUE", "SEMICOLON"}},
        {"Stmt", {"RETURN", "ReturnExpOpt", "SEMICOLON"}},
        {"ElseOpt", {"ELSE", "Stmt"}},
        {"ElseOpt", {}},
        {"ReturnExpOpt", {"Exp"}},
        {"ReturnExpOpt", {}},

        {"Exp", {"LOrExp"}},
        {"LVal", {"IDENT", "LValArrayDims"}},
        {"LValArrayDims", {"LBRACKET", "Exp", "RBRACKET", "LValArrayDims"}},
        {"LValArrayDims", {}},
        {"PrimaryExp", {"LPAREN", "Exp", "RPAREN"}},
        {"PrimaryExp", {"LVal"}},
        {"PrimaryExp", {"Number"}},
        {"Number", {"INT_CONST"}},
        {"UnaryExp", {"IDENT", "LPAREN", "FuncRParamsOpt", "RPAREN"}},
        {"UnaryExp", {"PrimaryExp"}},
        {"UnaryExp", {"UnaryOp", "UnaryExp"}},
        {"UnaryOp", {"PLUS"}},
        {"UnaryOp", {"MINUS"}},
        {"UnaryOp", {"NOT"}},
        {"FuncRParamsOpt", {"FuncRParams"}},
        {"FuncRParamsOpt", {}},
        {"FuncRParams", {"Exp", "FuncRParamsTail"}},
        {"FuncRParamsTail", {"COMMA", "Exp", "FuncRParamsTail"}},
        {"FuncRParamsTail", {}},

        {"MulExp", {"UnaryExp", "MulExpTail"}},
        {"MulExpTail", {"STAR", "UnaryExp", "MulExpTail"}},
        {"MulExpTail", {"SLASH", "UnaryExp", "MulExpTail"}},
        {"MulExpTail", {"PERCENT", "UnaryExp", "MulExpTail"}},
        {"MulExpTail", {}},
        {"AddExp", {"MulExp", "AddExpTail"}},
        {"AddExpTail", {"PLUS", "MulExp", "AddExpTail"}},
        {"AddExpTail", {"MINUS", "MulExp", "AddExpTail"}},
        {"AddExpTail", {}},
        {"RelExp", {"AddExp", "RelExpTail"}},
        {"RelExpTail", {"LT", "AddExp", "RelExpTail"}},
        {"RelExpTail", {"GT", "AddExp", "RelExpTail"}},
        {"RelExpTail", {"LE", "AddExp", "RelExpTail"}},
        {"RelExpTail", {"GE", "AddExp", "RelExpTail"}},
        {"RelExpTail", {}},
        {"EqExp", {"RelExp", "EqExpTail"}},
        {"EqExpTail", {"EQ", "RelExp", "EqExpTail"}},
        {"EqExpTail", {"NE", "RelExp", "EqExpTail"}},
        {"EqExpTail", {}},
        {"LAndExp", {"EqExp", "LAndExpTail"}},
        {"LAndExpTail", {"AND", "EqExp", "LAndExpTail"}},
        {"LAndExpTail", {}},
        {"LOrExp", {"LAndExp", "LOrExpTail"}},
        {"LOrExpTail", {"OR", "LAndExp", "LOrExpTail"}},
        {"LOrExpTail", {}},
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
