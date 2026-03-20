#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

void register_test(const std::string& name, std::function<void()> fn) {
  registry().push_back({name, std::move(fn)});
}

#define MIMIC_TEST(name)                        \
  void name();                                  \
  namespace {                                   \
  struct name##_registrar {                     \
    name##_registrar() { register_test(#name, name); } \
  } name##_instance;                            \
  }                                             \
  void name()

#define MIMIC_ASSERT(expr)                                                \
  do {                                                                     \
    if (!(expr)) {                                                         \
      throw std::runtime_error(std::string("Assertion failed: ") + #expr); \
    }                                                                      \
  } while (false)

#include "test_command_registry.cpp"
#include "test_workspace_model.cpp"
#include "test_overview_controller.cpp"

int main() {
  std::size_t failed = 0;
  for (const auto& test : registry()) {
    try {
      test.fn();
      std::cout << "PASS " << test.name << '\n';
    } catch (const std::exception& e) {
      ++failed;
      std::cerr << "FAIL " << test.name << ": " << e.what() << '\n';
    }
  }

  if (failed > 0) {
    std::cerr << failed << " test(s) failed\n";
    return 1;
  }

  std::cout << "All tests passed: " << registry().size() << '\n';
  return 0;
}
