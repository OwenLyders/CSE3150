// Runs every TEST, or only one area's tests:
//
//   cachesim_tests          run everything
//   cachesim_tests cache    run only tests/test_cache.cpp
//
// ctest runs one area per test (see CMakeLists.txt). Exits non-zero if anything fails.

#include "TestFramework.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace testing {

namespace {

std::vector<std::string>* currentFailures = nullptr;

std::string fileName(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;  // function-local, so it exists before any Registrar runs
    return tests;
}

void recordFailure(const char* file, int line, const std::string& message) {
    currentFailures->push_back(fileName(file) + ":" + std::to_string(line) + ": " + message);
}

std::string areaFromPath(const char* path) {
    std::string name = fileName(path);
    if (name.rfind("test_", 0) == 0) name = name.substr(5);
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

}  // namespace testing

int main(int argc, char* argv[]) {
    const std::string area = argc > 1 ? argv[1] : "";
    int passed = 0;
    int failed = 0;

    for (const testing::TestCase& test : testing::registry()) {
        if (!area.empty() && test.area != area) continue;

        std::vector<std::string> failures;
        testing::currentFailures = &failures;
        try {
            test.run();
        } catch (const std::exception& error) {
            failures.push_back(std::string("unexpected exception: ") + error.what());
        } catch (...) {
            failures.push_back("unexpected exception that is not a std::exception");
        }

        if (failures.empty()) {
            ++passed;
            std::cout << "  pass  [" << test.area << "] " << test.name << "\n";
        } else {
            ++failed;
            std::cout << "  FAIL  [" << test.area << "] " << test.name << "\n";
            for (const std::string& failure : failures) std::cout << "        " << failure << "\n";
        }
    }

    if (passed + failed == 0) {
        std::cout << "no tests found for area '" << area << "'\n";
        return 1;
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
