#pragma once

#include "core/MemRequest.h"

#include <cstddef>
#include <cstdint>
#include <string>

// Where an address lands in the cache:  | tag | set index | offset |
struct AddressParts {
    std::uint32_t tag = 0;
    std::size_t index = 0;   // which set
    std::size_t offset = 0;  // which byte inside the block
};

// The four settings that define a cache, and the bit widths they imply.
//
// With the defaults (16-bit addresses, 4-byte blocks, a 64-byte cache, 4 ways) the cache
// holds 16 blocks in 4 sets of 4, so an address is 12 tag bits, 2 index bits, 2 offset bits.
//
// Block size, cache size and associativity must each be a power of two. The address width
// is a bit count, so it is range-checked instead. Main memory must be at least twice the
// cache. Call validate() before using the rest; Cache and Simulator do this for you.
struct CacheConfig {
    unsigned addressBits = 16;      // main memory is 2^addressBits bytes
    std::size_t blockSize = 4;      // bytes per block (one cache line's worth of data)
    std::size_t cacheSize = 64;     // total bytes of data the cache holds
    std::size_t associativity = 4;  // lines (ways) per set

    // Every cache keeps its own copy of main memory, so cap memory at 16 MiB. Bigger caches
    // make the built-in traces (which grow with the cache) too long, so cap those at 64 KiB.
    static constexpr unsigned kMaxAddressBits = 24;
    static constexpr std::size_t kMaxCacheSize = 64 * 1024;

    // Throws std::invalid_argument naming the first problem it finds.
    void validate() const;

    std::size_t memorySize() const;  // 2^addressBits
    std::size_t numBlocks() const;   // cacheSize / blockSize
    std::size_t numSets() const;     // numBlocks / associativity

    unsigned offsetBits() const;  // log2(blockSize)
    unsigned indexBits() const;   // log2(numSets)
    unsigned tagBits() const;     // whatever is left of the address

    AddressParts split(Address address) const;

    // The first byte of the block with this tag in this set (split() in reverse, offset 0).
    Address blockAddress(std::uint32_t tag, std::size_t setIndex) const;

    // For example "64 B cache, 4 B blocks, 4-way set associative (4 sets x 4 ways)".
    std::string describe() const;
};
