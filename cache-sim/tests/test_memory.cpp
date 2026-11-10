#include "TestFramework.h"

#include "core/MainMemory.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

TEST(each_byte_starts_as_low_byte_of_its_address) {
    const MainMemory memory(65536);
    CHECK_EQ(memory.size(), 65536);
    CHECK_EQ(memory.readByte(0x0000), 0x00);
    CHECK_EQ(memory.readByte(0x1A2E), 0x2E);
    CHECK_EQ(memory.readByte(0x00FF), 0xFF);
    CHECK_EQ(memory.readByte(0x0100), 0x00);
}

TEST(read_block_returns_consecutive_bytes) {
    const MainMemory memory(65536);
    const std::vector<std::uint8_t> expected{0x2C, 0x2D, 0x2E, 0x2F};
    CHECK_EQ(memory.readBlock(0x1A2C, 4), expected);
}

TEST(write_block_replaces_only_those_bytes) {
    MainMemory memory(256);
    memory.writeBlock(0x10, {0xAA, 0xBB});
    CHECK_EQ(memory.readByte(0x10), 0xAA);
    CHECK_EQ(memory.readByte(0x11), 0xBB);
    CHECK_EQ(memory.readByte(0x0F), 0x0F);
    CHECK_EQ(memory.readByte(0x12), 0x12);
}

TEST(access_outside_memory_throws) {
    MainMemory memory(256);
    CHECK_THROWS(memory.readByte(256), std::out_of_range);
    CHECK_THROWS(memory.readBlock(254, 4), std::out_of_range);
    CHECK_THROWS(memory.writeBlock(255, {1, 2}), std::out_of_range);
    CHECK_EQ(memory.readBlock(252, 4).size(), 4);  // the last whole block is fine
}
