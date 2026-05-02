#pragma once

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace compiler::tests {

struct TestCase {
  const char *name;
  void (*run)();
};

inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

struct TestRegistrar {
  TestRegistrar(const char *name, void (*run)()) {
    registry().push_back({name, run});
  }
};

inline void fail(const char *file, int line, const std::string &message) {
  std::ostringstream output;
  output << file << ':' << line << ": " << message;
  throw std::runtime_error(output.str());
}

template <typename Left, typename Right>
void expectEqual(const Left &left, const Right &right, const char *left_expr,
                 const char *right_expr, const char *file, int line) {
  if (left == right) {
    return;
  }
  std::ostringstream message;
  message << "expected " << left_expr << " == " << right_expr << ", got "
          << left << " and " << right;
  fail(file, line, message.str());
}

inline void expectTrue(bool value, const char *expr, const char *file,
                       int line) {
  if (!value) {
    fail(file, line, std::string("expected true: ") + expr);
  }
}

inline int runAllTests() {
  int failed = 0;
  for (const TestCase &test : registry()) {
    try {
      test.run();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception &error) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << '\n'
                << "       " << error.what() << '\n';
    }
  }

  std::cout << registry().size() - failed << '/' << registry().size()
            << " tests passed\n";
  return failed == 0 ? 0 : 1;
}

} // namespace compiler::tests

#define TEST_CASE(name)                                                        \
  static void name();                                                          \
  static ::compiler::tests::TestRegistrar name##_registrar(#name, &name);      \
  static void name()

#define EXPECT_TRUE(expr)                                                      \
  ::compiler::tests::expectTrue(static_cast<bool>(expr), #expr, __FILE__,      \
                                __LINE__)

#define EXPECT_EQ(left, right)                                                 \
  ::compiler::tests::expectEqual((left), (right), #left, #right, __FILE__,     \
                                 __LINE__)
