#pragma once

// A tiny test framework, so the tests need nothing beyond the standard library.
//
//   TEST(first_access_misses) {
//       CHECK(!result.hit);
//       CHECK_EQ(cache.stats().misses, 1);
//   }
//
// TEST registers a function and test_main.cpp runs it. CHECK and CHECK_EQ record a
// failure and keep going, so one run reports every broken check. Unlike assert(),
// they still run in release builds.

#include <cstddef>
#include <exception>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::string area;  // the test file's name without "test_" and ".cpp", e.g. "cache"
    void (*run)();
};

std::vector<TestCase>& registry();
void recordFailure(const char* file, int line, const std::string& message);
std::string areaFromPath(const char* path);

struct Registrar {
    Registrar(const char* name, const char* file, void (*run)()) {
        registry().push_back({name, areaFromPath(file), run});
    }
};

// A value as text, for failure messages. A uint8_t would print as a character, so it is
// shown as a number instead.
template <typename T>
std::string show(const T& value) {
    std::ostringstream out;
    if constexpr (std::is_same_v<T, bool>) {
        out << (value ? "true" : "false");
    } else if constexpr (std::is_same_v<T, unsigned char> || std::is_same_v<T, signed char>) {
        out << static_cast<int>(value);
    } else {
        out << value;
    }
    return out.str();
}

template <typename T>
std::string show(const std::vector<T>& values) {
    std::string text = "{";
    for (std::size_t i = 0; i < values.size(); ++i) {
        text += (i == 0 ? "" : ", ") + show(values[i]);
    }
    return text + "}";
}

// a == b, except that two integers are both converted to long long first. That lets a test
// write CHECK_EQ(stats.hits, 2), comparing a size_t with an int, without a signed/unsigned
// warning. (Fine for the small numbers these tests use.)
template <typename A, typename B>
bool equal(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return static_cast<long long>(a) == static_cast<long long>(b);
    } else {
        return a == b;
    }
}

template <typename A, typename B>
void checkEqual(const A& actual, const B& expected, const char* actualText, const char* expectedText,
                const char* file, int line) {
    if (!equal(actual, expected)) {
        recordFailure(file, line,
                      std::string("CHECK_EQ(") + actualText + ", " + expectedText + ")\n" +
                          "          actual:   " + show(actual) + "\n" +
                          "          expected: " + show(expected));
    }
}

// Runs f and returns what() of the std::exception it throws, or "" if it throws nothing.
template <typename F>
std::string thrownMessage(F&& f) {
    try {
        f();
    } catch (const std::exception& error) {
        return error.what();
    }
    return "";
}

}  // namespace testing

#define TEST(name)                                                              \
    static void name();                                                         \
    static const ::testing::Registrar name##_registrar(#name, __FILE__, &name); \
    static void name()

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) ::testing::recordFailure(__FILE__, __LINE__, "CHECK(" #condition ")"); \
    } while (false)

#define CHECK_EQ(actual, expected) \
    ::testing::checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define CHECK_THROWS(expression, ExceptionType)                                              \
    do {                                                                                     \
        bool threw_ = false;                                                                 \
        try {                                                                                \
            (void)(expression);                                                              \
        } catch (const ExceptionType&) {                                                     \
            threw_ = true;                                                                   \
        } catch (...) {                                                                      \
        }                                                                                    \
        if (!threw_)                                                                         \
            ::testing::recordFailure(__FILE__, __LINE__,                                     \
                                     "CHECK_THROWS(" #expression "): expected " #ExceptionType); \
    } while (false)
