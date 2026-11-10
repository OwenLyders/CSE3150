#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// What one request did in one cache. The GUI draws from it; the command-line runner
// only needs the totals in CacheStats.
struct AccessResult {
    bool hit = false;
    std::size_t setIndex = 0;                 // the set the address maps to
    std::size_t way = 0;                      // the line that was hit or filled
    std::uint32_t tag = 0;
    std::size_t offset = 0;
    std::uint8_t value = 0;                   // the byte read, or the byte written
    std::optional<std::uint32_t> evictedTag;  // set when the miss pushed out a valid block
    bool wroteBack = false;                   // that block was dirty and went back to memory
};
