#include "compiler/ir/opt/util/ir_opt_utils.h"

#include "compiler/ir/koopa_generator.h"

#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace compiler::ir::opt {

std::string trim(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool isInteger(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  size_t index = text[0] == '-' ? 1 : 0;
  if (index == text.size()) {
    return false;
  }
  for (; index < text.size(); ++index) {
    if (!std::isdigit(static_cast<unsigned char>(text[index]))) {
      return false;
    }
  }
  return true;
}

bool isValueName(const std::string &text) {
  return startsWith(text, "%") || startsWith(text, "@");
}

bool isLabel(const std::string &line) {
  return !line.empty() && line.back() == ':';
}

bool isTerminator(const std::string &line) {
  return startsWith(line, "ret") || startsWith(line, "jump ") ||
         startsWith(line, "br ");
}

std::vector<std::string> splitWhitespace(const std::string &text) {
  std::istringstream input(text);
  std::vector<std::string> parts;
  std::string part;
  while (input >> part) {
    if (!part.empty() && part.back() == ',') {
      part.pop_back();
    }
    parts.push_back(part);
  }
  return parts;
}

std::vector<std::string> collectValueNames(const std::string &text) {
  std::vector<std::string> names;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%' && text[i] != '@') {
      continue;
    }
    size_t begin = i;
    ++i;
    while (i < text.size() &&
           (std::isalnum(static_cast<unsigned char>(text[i])) ||
            text[i] == '_')) {
      ++i;
    }
    names.push_back(text.substr(begin, i - begin));
    if (i > begin) {
      --i;
    }
  }
  return names;
}

std::set<std::string>
collectValuesUsedOutsideDefiningBlock(const IrFunction &function) {
  std::map<std::string, int> definition_blocks;
  std::map<std::string, std::set<int>> use_blocks;
  int block = 0;

  for (const std::string &line : function.instructions) {
    if (isLabel(line)) {
      ++block;
      continue;
    }

    Assignment assignment = parseAssignment(line);
    if (assignment.valid) {
      definition_blocks[assignment.result] = block;
      for (const std::string &arg : assignment.args) {
        if (isValueName(arg)) {
          use_blocks[arg].insert(block);
        }
      }
      continue;
    }

    for (const std::string &name : collectValueNames(line)) {
      use_blocks[name].insert(block);
    }
  }

  std::set<std::string> outside;
  for (const auto &entry : use_blocks) {
    auto definition = definition_blocks.find(entry.first);
    if (definition == definition_blocks.end()) {
      continue;
    }
    for (int use_block : entry.second) {
      if (use_block != definition->second) {
        outside.insert(entry.first);
        break;
      }
    }
  }
  return outside;
}

Assignment parseAssignment(const std::string &line) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() < 3 || parts[1] != "=") {
    return {};
  }
  Assignment assignment;
  assignment.valid = true;
  assignment.result = parts[0];
  assignment.op = parts[2];
  assignment.args.assign(parts.begin() + 3, parts.end());
  return assignment;
}

std::string formatAssignment(const Assignment &assignment) {
  std::ostringstream output;
  output << assignment.result << " = " << assignment.op;
  for (size_t i = 0; i < assignment.args.size(); ++i) {
    output << (i == 0 ? " " : ", ") << assignment.args[i];
  }
  return output.str();
}

bool isSideEffectFree(const Assignment &assignment) {
  if (!assignment.valid) {
    return false;
  }
  return assignment.op == "add" || assignment.op == "sub" ||
         assignment.op == "mul" || assignment.op == "div" ||
         assignment.op == "mod" || assignment.op == "eq" ||
         assignment.op == "ne" || assignment.op == "lt" ||
         assignment.op == "gt" || assignment.op == "le" ||
         assignment.op == "ge" || assignment.op == "load" ||
         assignment.op == "getelemptr" || assignment.op == "getptr";
}

std::string replaceOperands(
    const std::string &line,
    const std::vector<std::pair<std::string, std::string>> &replacements) {
  std::map<std::string, std::string> replacement_map;
  for (const auto &replacement : replacements) {
    replacement_map[replacement.first] = replacement.second;
  }
  return replaceOperands(line, replacement_map);
}

std::string replaceOperands(
    const std::string &line,
    const std::map<std::string, std::string> &replacements) {
  std::string result;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] != '%' && line[i] != '@') {
      result.push_back(line[i]);
      continue;
    }
    size_t begin = i;
    ++i;
    while (i < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[i])) ||
            line[i] == '_')) {
      ++i;
    }
    std::string name = line.substr(begin, i - begin);
    auto found = replacements.find(name);
    result += found == replacements.end() ? name : found->second;
    if (i < line.size()) {
      --i;
    }
  }
  return result;
}

long long evaluateBinary(const std::string &op, long long lhs, long long rhs) {
  if (op == "add") return lhs + rhs;
  if (op == "sub") return lhs - rhs;
  if (op == "mul") return lhs * rhs;
  if (op == "div") return rhs == 0 ? 0 : lhs / rhs;
  if (op == "mod") return rhs == 0 ? 0 : lhs % rhs;
  if (op == "eq") return lhs == rhs;
  if (op == "ne") return lhs != rhs;
  if (op == "lt") return lhs < rhs;
  if (op == "gt") return lhs > rhs;
  if (op == "le") return lhs <= rhs;
  if (op == "ge") return lhs >= rhs;
  throw IrError("unsupported constant binary op: " + op);
}

} // namespace compiler::ir::opt
