#include "compiler/ir/opt/passes/value_passes.h"

#include <cctype>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace compiler::ir::opt {
namespace {

std::string readSymbolName(const std::string &line, size_t at) {
  size_t end = at + 1;
  while (end < line.size() &&
         (std::isalnum(static_cast<unsigned char>(line[end])) ||
          line[end] == '_')) {
    ++end;
  }
  return line.substr(at, end - at);
}

std::string functionName(const IrFunction &function) {
  size_t at = function.header.find('@');
  if (at == std::string::npos) {
    return {};
  }
  return readSymbolName(function.header, at);
}

std::set<std::string> collectCalledFunctions(const IrFunction &function) {
  std::set<std::string> calls;
  for (const std::string &line : function.instructions) {
    size_t call = line.find("call @");
    while (call != std::string::npos) {
      calls.insert(readSymbolName(line, call + 5));
      call = line.find("call @", call + 1);
    }
  }
  return calls;
}

std::set<std::string>
reachableFunctions(const std::map<std::string, std::set<std::string>> &calls) {
  std::set<std::string> reachable;
  if (calls.find("@main") == calls.end()) {
    return reachable;
  }

  std::vector<std::string> stack = {"@main"};
  while (!stack.empty()) {
    std::string current = stack.back();
    stack.pop_back();
    if (!reachable.insert(current).second) {
      continue;
    }

    auto callees = calls.find(current);
    if (callees == calls.end()) {
      continue;
    }
    for (const std::string &callee : callees->second) {
      if (calls.find(callee) != calls.end()) {
        stack.push_back(callee);
      }
    }
  }
  return reachable;
}

} // namespace

PassResult UnusedFunctionEliminationPass::run(IrModule &module) {
  std::map<std::string, std::set<std::string>> calls;
  for (const IrFunction &function : module.functions) {
    calls[functionName(function)] = collectCalledFunctions(function);
  }

  std::set<std::string> reachable = reachableFunctions(calls);
  if (reachable.empty()) {
    return {};
  }

  PassResult result;
  std::vector<IrFunction> optimized;
  for (IrFunction &function : module.functions) {
    if (reachable.find(functionName(function)) == reachable.end()) {
      result.changed = true;
      continue;
    }
    optimized.push_back(std::move(function));
  }
  module.functions = std::move(optimized);
  return result;
}

} // namespace compiler::ir::opt
