#pragma once

#include "core/ReplacementPolicy.h"

// First in, first out: evicts the block that was loaded earliest.
// Hits don't count, so a line is stamped only when a block is loaded into it.
class FifoPolicy : public ReplacementPolicy {
public:
    std::string name() const override { return "FIFO"; }

    void onLoad(CacheLine& line) override { line.stamp = nextStamp(); }

    void onHit(CacheLine& /*line*/) override {}  // FIFO ignores hits

    std::size_t chooseVictim(const std::vector<CacheLine>& set) const override {
        return lineWithSmallestStamp(set);
    }
};
