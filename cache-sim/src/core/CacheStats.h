#pragma once

#include <cstddef>

// Running counters for one cache.
struct CacheStats {
    std::size_t accesses = 0;
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t evictions = 0;   // misses that had to push out a valid block
    std::size_t writeBacks = 0;  // evictions of dirty blocks

    // hits / accesses, or 0 before the first access.
    double hitRate() const {
        return accesses == 0 ? 0.0 : static_cast<double>(hits) / static_cast<double>(accesses);
    }
};
