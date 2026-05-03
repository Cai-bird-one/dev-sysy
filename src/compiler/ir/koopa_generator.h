#pragma once

#include "compiler/parser/parser.h"

#include <iosfwd>
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
};

} // namespace compiler::ir
