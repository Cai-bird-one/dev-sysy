#include "compiler/ir/emit/program/program_context.h"
#include "compiler/ir/koopa_generator.h"
#include "tests/test_framework.h"

#include <map>
#include <set>
#include <string>

using namespace compiler;

TEST_CASE(program_context_tracks_globals_and_unique_values) {
  ir::ProgramContext context;

  std::string first = context.newGlobalValue("x");
  std::string second = context.newGlobalValue("x");
  context.defineGlobal(
      "x", ir::Symbol{ir::SymbolKind::Variable, 0, first, {}, true, false});
  context.addGlobalInstruction("global " + first + " = alloc i32, zeroinit");

  EXPECT_EQ(first, "@x");
  EXPECT_EQ(second, "@x_1");
  EXPECT_EQ(context.lookupGlobal("x").pointer, "@x");
  EXPECT_EQ(context.globalInstructions().size(), 1);
  EXPECT_EQ(context.globalInstructions()[0], "global @x = alloc i32, zeroinit");
}

TEST_CASE(program_context_tracks_function_signatures_and_external_use) {
  ir::ProgramContext context;

  std::map<std::string, ir::FunctionSignature> signatures;
  signatures["getint"] = ir::FunctionSignature{"i32", 0, true, {}};
  context.setFunctionSignatures(std::move(signatures));
  context.usedExternalFunctions().insert("getint");

  EXPECT_EQ(context.functionSignature("getint").return_type, "i32");
  EXPECT_TRUE(context.functionSignature("getint").external);
  EXPECT_TRUE(context.usedExternalFunctions().find("getint") !=
              context.usedExternalFunctions().end());
}

TEST_CASE(program_context_rejects_duplicate_global_names) {
  ir::ProgramContext context;
  context.defineGlobal(
      "x", ir::Symbol{ir::SymbolKind::Variable, 0, "@x", {}, true, false});

  bool rejected = false;
  try {
    context.defineGlobal(
        "x", ir::Symbol{ir::SymbolKind::Variable, 0, "@x_1", {}, true,
                        false});
  } catch (const ir::IrError &) {
    rejected = true;
  }

  EXPECT_TRUE(rejected);
}
