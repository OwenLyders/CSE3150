#pragma once

#include "core/ReplacementPolicy.h"

// Least recently used: evicts the block that has gone longest without being touched.
// Exactly FifoPolicy except for one line: a hit re-stamps the line.
class LruPolicy : public ReplacementPolicy {
public:
    std::string name() const override { return "LRU"; }

    void onLoad(CacheLine& line) override { line.stamp = nextStamp(); }

    void onHit(CacheLine& line) override { line.stamp = nextStamp(); }  // the line that differs

    std::size_t chooseVictim(const std::vector<CacheLine>& set) const override {
        return lineWithSmallestStamp(set);
    }
};
