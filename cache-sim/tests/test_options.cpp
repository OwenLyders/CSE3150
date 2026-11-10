#include "TestFramework.h"

#include "core/Options.h"

#include <string>
#include <vector>

namespace {

std::string errorFor(const std::vector<std::string>& args) {
    return testing::thrownMessage([&] { parseOptions(args); });
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

TEST(no_flags_gives_design_plan_defaults) {
    const Options options = parseOptions({});
    CHECK_EQ(options.config.cacheSize, 64);
    CHECK_EQ(options.config.blockSize, 4);
    CHECK_EQ(options.config.associativity, 4);
    CHECK_EQ(options.config.addressBits, 16);
    CHECK(!options.traceFile);
    CHECK(!options.builtin);
    CHECK(!options.showHelp);
}

TEST(reads_cache_settings) {
    const Options options =
        parseOptions({"--cache", "128", "--block", "8", "--ways", "2", "--address-bits", "12"});
    CHECK_EQ(options.config.cacheSize, 128);
    CHECK_EQ(options.config.blockSize, 8);
    CHECK_EQ(options.config.associativity, 2);
    CHECK_EQ(options.config.addressBits, 12);
}

TEST(accepts_flag_equals_value) {
    const Options options = parseOptions({"--cache=128", "--ways=0x8"});
    CHECK_EQ(options.config.cacheSize, 128);
    CHECK_EQ(options.config.associativity, 8);
}

TEST(reads_the_trace_choice) {
    CHECK_EQ(parseOptions({"--trace", "traces/example.trace"}).traceFile.value_or(""), "traces/example.trace");
    CHECK_EQ(parseOptions({"--builtin", "hot-cold"}).builtin.value_or(""), "hot-cold");
}

TEST(help_flag) {
    CHECK(parseOptions({"--help"}).showHelp);
    CHECK(parseOptions({"-h"}).showHelp);
    CHECK(contains(optionsHelp(), "--ways"));
}

TEST(rejects_an_unknown_flag) {
    CHECK(contains(errorFor({"--cahce", "64"}), "--cahce"));
}

TEST(rejects_a_missing_value) {
    CHECK(contains(errorFor({"--cache"}), "--cache"));
}

TEST(rejects_values_that_are_not_whole_numbers) {
    CHECK(contains(errorFor({"--ways", "four"}), "four"));
    CHECK(contains(errorFor({"--cache", "-64"}), "-64"));
    CHECK(!errorFor({"--address-bits", "4294967312"}).empty());  // must not wrap around to 16
}

TEST(rejects_invalid_cache_settings) {
    CHECK(contains(errorFor({"--ways", "3"}), "power of two"));
    CHECK(contains(errorFor({"--block", "128"}), "larger than the cache"));
}

TEST(rejects_an_unknown_builtin_and_lists_the_choices) {
    const std::string message = errorFor({"--builtin", "nope"});
    CHECK(contains(message, "nope"));
    CHECK(contains(message, "hot-cold"));
}

TEST(rejects_trace_and_builtin_together) {
    CHECK(!errorFor({"--trace", "a.trace", "--builtin", "random"}).empty());
}
