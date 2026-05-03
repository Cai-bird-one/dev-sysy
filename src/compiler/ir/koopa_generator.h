#pragma once

#include "compiler/parser/parser.h"

#include <iosfwd>
#include <map>
#include <stdexcept>
#include <string>

namespace compiler::ir {

class IrError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class KoopaGenerator {
public:
  std::string generate(const compiler::parser::ParseNode &ast) const;
  void generate(const compiler::parser::ParseNode &ast,
                std::ostream &output) const;

private:
  std::string findFunctionName(const compiler::parser::ParseNode &ast) const;
  std::string findReturnValue(const compiler::parser::ParseNode &ast) const;
  void collectGlobalDeclarations(const compiler::parser::ParseNode &node,
                                 std::map<std::string, long long> &symbols)
      const;
  long long evaluateExpression(const compiler::parser::ParseNode &node,
                               const std::map<std::string, long long> &symbols)
      const;
  long long evaluateAddExpTail(
      const compiler::parser::ParseNode &node, long long lhs,
      const std::map<std::string, long long> &symbols) const;
  long long evaluateRelExpTail(
      const compiler::parser::ParseNode &node, long long lhs,
      const std::map<std::string, long long> &symbols) const;
  long long evaluateEqExpTail(
      const compiler::parser::ParseNode &node, long long lhs,
      const std::map<std::string, long long> &symbols) const;
  long long evaluateLAndExpTail(
      const compiler::parser::ParseNode &node, long long lhs,
      const std::map<std::string, long long> &symbols) const;
  long long evaluateLOrExpTail(
      const compiler::parser::ParseNode &node, long long lhs,
      const std::map<std::string, long long> &symbols) const;
  long long evaluateMulExpTail(
      const compiler::parser::ParseNode &node, long long lhs,
      const std::map<std::string, long long> &symbols) const;
  void collectBlockItems(const compiler::parser::ParseNode &node,
                         std::map<std::string, long long> &symbols,
                         const compiler::parser::ParseNode *&return_exp) const;
  void collectDeclaration(const compiler::parser::ParseNode &node,
                          std::map<std::string, long long> &symbols) const;
};

} // namespace compiler::ir
