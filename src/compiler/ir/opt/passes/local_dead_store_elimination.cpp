#include "compiler/ir/opt/passes/value_passes.h"

#include "compiler/ir/opt/util/ir_opt_utils.h"

#include <set>
#include <utility>
#include <vector>

namespace compiler::ir::opt {
namespace {

std::set<std::string> collectScalarAllocs(const IrFunction &function) {
  std::set<std::string> allocs;
  for (const std::string &line : function.instructions) {
    Assignment assignment = parseAssignment(line);
    if (assignment.valid && assignment.op == "alloc" &&
        assignment.args.size() == 1 && assignment.args[0] == "i32") {
      allocs.insert(assignment.result);
    }
  }
  return allocs;
}

bool isScalarPointer(const std::string &value,
                     const std::set<std::string> &scalar_allocs) {
  return scalar_allocs.find(value) != scalar_allocs.end();
}

bool isCall(const Assignment &assignment, const std::vector<std::string> &parts) {
  return (assignment.valid && assignment.op == "call") ||
         (!parts.empty() && parts[0] == "call");
}

bool isStoreToScalar(const std::string &line,
                     const std::set<std::string> &scalar_allocs,
                     std::string &pointer) {
  std::vector<std::string> parts = splitWhitespace(line);
  if (parts.size() == 3 && parts[0] == "store" &&
      isScalarPointer(parts[2], scalar_allocs)) {
    pointer = parts[2];
    return true;
  }
  return false;
}

bool isLoadFromScalar(const Assignment &assignment,
                      const std::set<std::string> &scalar_allocs,
                      std::string &pointer) {
  if (assignment.valid && assignment.op == "load" &&
      assignment.args.size() == 1 &&
      isScalarPointer(assignment.args[0], scalar_allocs)) {
    pointer = assignment.args[0];
    return true;
  }
  return false;
}

std::set<std::string> allLive(const std::set<std::string> &scalar_allocs) {
  return scalar_allocs;
}

} // namespace

PassResult LocalDeadStoreEliminationPass::run(IrFunction &function) {
  PassResult result;
  std::set<std::string> scalar_allocs = collectScalarAllocs(function);
  std::set<std::string> live_memory;
  std::vector<std::string> reversed;

  for (auto it = function.instructions.rbegin();
       it != function.instructions.rend(); ++it) {
    const std::string &line = *it;
    std::vector<std::string> parts = splitWhitespace(line);
    Assignment assignment = parseAssignment(line);

    if (parts.size() == 2 && parts[0] == "ret") {
      live_memory.clear();
      reversed.push_back(line);
      continue;
    }
    if ((parts.size() == 2 && parts[0] == "jump") ||
        (parts.size() == 4 && parts[0] == "br")) {
      live_memory = allLive(scalar_allocs);
      reversed.push_back(line);
      continue;
    }
    if (isLabel(line)) {
      live_memory = allLive(scalar_allocs);
      reversed.push_back(line);
      continue;
    }
    if (isCall(assignment, parts)) {
      live_memory = allLive(scalar_allocs);
      reversed.push_back(line);
      continue;
    }

    std::string pointer;
    if (isStoreToScalar(line, scalar_allocs, pointer)) {
      if (live_memory.find(pointer) == live_memory.end()) {
        result.changed = true;
        continue;
      }
      live_memory.erase(pointer);
      reversed.push_back(line);
      continue;
    }
    if (isLoadFromScalar(assignment, scalar_allocs, pointer)) {
      live_memory.insert(pointer);
      reversed.push_back(line);
      continue;
    }

    reversed.push_back(line);
  }

  function.instructions.assign(reversed.rbegin(), reversed.rend());
  return result;
}

} // namespace compiler::ir::opt
