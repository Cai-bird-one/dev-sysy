#pragma once

#include "compiler/parser/parser.h"

#include <any>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace compiler::ir::sdt {

class AttributeSet {
public:
  template <typename T> void set(std::string name, T value) {
    values_[std::move(name)] = std::move(value);
  }

  template <typename T> const T &get(const std::string &name) const {
    auto found = values_.find(name);
    if (found == values_.end()) {
      throw std::runtime_error("missing SDT attribute: " + name);
    }
    return std::any_cast<const T &>(found->second);
  }

  bool has(const std::string &name) const {
    return values_.find(name) != values_.end();
  }

private:
  std::map<std::string, std::any> values_;
};

class SyntaxDirectedTranslator {
public:
  using Rule =
      std::function<AttributeSet(const compiler::parser::ParseNode &,
                                 const SyntaxDirectedTranslator &)>;

  SyntaxDirectedTranslator();

  void registerRule(int production_id, Rule rule);
  void setDefaultRule(Rule rule);
  AttributeSet translate(const compiler::parser::ParseNode &node) const;

private:
  std::map<int, Rule> rules_;
  Rule default_rule_;
};

} // namespace compiler::ir::sdt
