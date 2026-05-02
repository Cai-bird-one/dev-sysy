#pragma once

#include <string>
#include <unordered_map>
#include <utility>

namespace compiler {

struct ProcessContext {
  std::string input_path;
  std::string output_path;
  std::unordered_map<std::string, std::string> options;
};

class CompilerProcess {
public:
  explicit CompilerProcess(std::string name) : name_(std::move(name)) {}
  virtual ~CompilerProcess() = default;

  const std::string &name() const { return name_; }

  virtual bool run(ProcessContext &context) = 0;

private:
  std::string name_;
};

} // namespace compiler
