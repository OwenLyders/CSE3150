#include "TestFramework.h"

#include "core/CacheConfig.h"

#include <stdexcept>
#include <string>

namespace {

CacheConfig configWith(std::size_t cacheSize, std::size_t blockSize, std::size_t ways,
                       unsigned addressBits = 16) {
    CacheConfig config;
    config.cacheSize = cacheSize;
    config.blockSize = blockSize;
    config.associativity = ways;
    config.addressBits = addressBits;
    return config;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

TEST(defaults_match_design_plan) {
    const CacheConfig config;
    config.validate();
    CHECK_EQ(config.addressBits, 16);
    CHECK_EQ(config.blockSize, 4);
    CHECK_EQ(config.cacheSize, 64);
    CHECK_EQ(config.associativity, 4);
    CHECK_EQ(config.memorySize(), 65536);
    CHECK_EQ(config.numBlocks(), 16);
    CHECK_EQ(config.numSets(), 4);
}

TEST(default_bit_widths) {
    const CacheConfig config;
    CHECK_EQ(config.offsetBits(), 2);
    CHECK_EQ(config.indexBits(), 2);
    CHECK_EQ(config.tagBits(), 12);
}

// The example from the design plan.
TEST(address_split_matches_design_plan) {
    const AddressParts parts = CacheConfig{}.split(0x1A2E);
    CHECK_EQ(parts.tag, 0x1A2);
    CHECK_EQ(parts.index, 3);
    CHECK_EQ(parts.offset, 2);
}

TEST(block_address_reverses_split_with_offset_zero) {
    const CacheConfig config;
    for (Address address : {0x0000u, 0x1A2Cu, 0x1A2Eu, 0x0013u, 0xFFFFu}) {
        const AddressParts parts = config.split(address);
        CHECK_EQ(config.blockAddress(parts.tag, parts.index), address & ~Address{3});
    }
}

TEST(direct_mapped_makes_every_line_its_own_set) {
    const CacheConfig config = configWith(64, 4, 1);
    config.validate();
    CHECK_EQ(config.numSets(), 16);
    CHECK_EQ(config.indexBits(), 4);
    CHECK_EQ(config.tagBits(), 10);
}

TEST(fully_associative_has_one_set_and_no_index_bits) {
    const CacheConfig config = configWith(64, 4, 16);
    config.validate();
    CHECK_EQ(config.numSets(), 1);
    CHECK_EQ(config.indexBits(), 0);
    CHECK_EQ(config.tagBits(), 14);
    CHECK_EQ(config.split(0x1A2E).index, 0);
    CHECK_EQ(config.split(0x1A2E).tag, 0x1A2E >> 2);
}

TEST(rejects_sizes_that_are_not_powers_of_two) {
    CHECK_THROWS(configWith(64, 3, 4).validate(), std::invalid_argument);
    CHECK_THROWS(configWith(100, 4, 4).validate(), std::invalid_argument);
    CHECK_THROWS(configWith(64, 4, 3).validate(), std::invalid_argument);
    CHECK_THROWS(configWith(64, 0, 4).validate(), std::invalid_argument);
    CHECK_THROWS(configWith(0, 4, 4).validate(), std::invalid_argument);
    CHECK_THROWS(configWith(64, 4, 0).validate(), std::invalid_argument);
}

TEST(rejects_block_larger_than_cache) {
    CHECK_THROWS(configWith(64, 128, 1).validate(), std::invalid_argument);
}

TEST(rejects_more_ways_than_blocks) {
    CHECK_THROWS(configWith(64, 4, 32).validate(), std::invalid_argument);
}

TEST(rejects_address_width_out_of_range) {
    CHECK_THROWS(configWith(64, 4, 4, 0).validate(), std::invalid_argument);
    CHECK_THROWS(configWith(64, 4, 4, 25).validate(), std::invalid_argument);
    configWith(64, 4, 4, 24).validate();  // the widest allowed
}

// Main memory must be at least twice the cache, or the built-in traces can't reach enough
// distinct blocks to cause evictions.
TEST(rejects_memory_smaller_than_twice_the_cache) {
    CHECK_THROWS(configWith(64, 4, 4, 5).validate(), std::invalid_argument);  // 32-byte memory
    CHECK_THROWS(configWith(64, 4, 4, 6).validate(), std::invalid_argument);  // 64-byte memory
    configWith(64, 4, 4, 7).validate();                                       // 128 bytes is enough
}

// Bigger caches make the built-in traces (and the history the GUI keeps) huge.
TEST(rejects_cache_larger_than_64_kib) {
    configWith(65536, 4, 4, 17).validate();  // 17 bits: memory twice the cache
    CHECK_THROWS(configWith(131072, 4, 4, 24).validate(), std::invalid_argument);
}

TEST(validation_message_names_the_bad_setting) {
    const std::string message = testing::thrownMessage([] { configWith(64, 3, 4).validate(); });
    CHECK(contains(message, "block size"));
    CHECK(contains(message, "3"));
}

TEST(describe_names_the_organization) {
    CHECK(contains(CacheConfig{}.describe(), "4-way"));
    CHECK(contains(configWith(64, 4, 1).describe(), "direct-mapped"));
    CHECK(contains(configWith(64, 4, 16).describe(), "fully associative"));
}
