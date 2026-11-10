#pragma once

#include "core/MemRequest.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// Byte-addressed backing store for one cache.
//
// Every byte starts out as the low 8 bits of its own address (address 0x1A2E holds 0x2E),
// so a block loaded into the cache shows where it came from, and a written byte stands out.
class MainMemory {
public:
    explicit MainMemory(std::size_t sizeBytes);

    std::size_t size() const;

    // All three throw std::out_of_range if any byte is outside memory.
    std::uint8_t readByte(Address address) const;
    std::vector<std::uint8_t> readBlock(Address start, std::size_t length) const;
    void writeBlock(Address start, const std::vector<std::uint8_t>& bytes);

private:
    void checkRange(Address start, std::size_t length) const;

    std::vector<std::uint8_t> bytes_;
};
