#include "compiler/ir/emit/function/function_context.h"
#include "compiler/ir/koopa_generator.h"
#include "tests/test_framework.h"

#include <map>
#include <set>
#include <string>

using namespace compiler;

namespace {

ir::FunctionContext makeContext(std::set<std::string> &used_external) {
  std::map<std::string, ir::Symbol> globals;
  globals["g"] =
      ir::Symbol{ir::SymbolKind::Variable, 0, "@g", {}, true, false};

  std::map<std::string, ir::FunctionSignature> signatures;
  signatures["putint"] = ir::FunctionSignature{"void", 1, true, {"i32"}};

  std::set<std::string> reserved = {"%x", "@main_a"};
  return ir::FunctionContext(std::move(globals), std::move(signatures),
                             std::move(reserved), used_external, "main",
                             "i32");
}

} // namespace

TEST_CASE(function_context_tracks_values_instructions_and_symbols) {
  std::set<std::string> used_external;
  ir::FunctionContext context = makeContext(used_external);

  std::string local = context.newNamedValue("x");
  std::string parameter = context.newParameterValue("a");
  context.emitLocalAlloc(local + " = alloc i32");
  context.emit(parameter + " = add 1, 2");
  context.define("x",
                 ir::Symbol{ir::SymbolKind::Variable, 0, local, {}, true,
                            false});

  EXPECT_EQ(local, "%x_1");
  EXPECT_EQ(parameter, "@main_a_1");
  EXPECT_EQ(context.lookup("g").pointer, "@g");
  EXPECT_EQ(context.lookup("x").pointer, "%x_1");
  EXPECT_EQ(context.entryAllocs()[0], "%x_1 = alloc i32");
  EXPECT_EQ(context.instructions()[0], "@main_a_1 = add 1, 2");
}

TEST_CASE(function_context_tracks_blocks_and_loop_labels) {
  std::set<std::string> used_external;
  ir::FunctionContext context = makeContext(used_external);

  std::string label = context.newLabel("while_entry");
  context.emit("jump " + label);
  EXPECT_TRUE(context.blockTerminated());

  context.emitLabel(label);
  EXPECT_TRUE(!context.blockTerminated());

  context.pushLoop(ir::StatementLoopLabels{"%break", "%continue"});
  EXPECT_TRUE(context.hasLoop());
  EXPECT_EQ(context.currentLoop().break_label, "%break");
  context.popLoop();
  EXPECT_TRUE(!context.hasLoop());
}

TEST_CASE(function_context_tracks_external_function_use) {
  std::set<std::string> used_external;
  ir::FunctionContext context = makeContext(used_external);

  EXPECT_EQ(context.lookupFunction("putint").parameter_count, 1);
  context.markExternalFunctionUsed("putint");

  EXPECT_TRUE(used_external.find("putint") != used_external.end());
}
