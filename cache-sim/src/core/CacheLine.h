#pragma once

#include <cstdint>
#include <vector>

// One line (way) in a set: a block's bytes plus the bookkeeping the cache and the policy need.
struct CacheLine {
    bool valid = false;              // holds a block
    bool dirty = false;              // written since it was loaded, so it must be written back on eviction
    std::uint32_t tag = 0;
    std::uint64_t stamp = 0;         // set by the replacement policy; the smallest stamp is evicted first
    std::vector<std::uint8_t> data;  // the block's bytes
};
