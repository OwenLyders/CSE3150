#pragma once

#include "core/CacheLine.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Decides which line to evict when a set is full.
//
// The cache calls onLoad() right after it puts a new block in a line, onHit() when a request
// finds its block already cached, and chooseVictim() when a miss needs a line and every line
// in the set is valid.
//
// FIFO and LRU both time-stamp lines and evict the smallest stamp. They differ only in
// whether a hit refreshes the stamp. A policy that works differently (Random, LFU) can
// ignore the stamp helpers and keep its own state.
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;

    virtual std::string name() const = 0;
    virtual void onLoad(CacheLine& line) = 0;
    virtual void onHit(CacheLine& line) = 0;

    // `set` is full. Returns the index of the line to evict. It is const because the GUI
    // also calls it to outline the next victim, which must not change what happens next.
    virtual std::size_t chooseVictim(const std::vector<CacheLine>& set) const = 0;

protected:
    // 1, 2, 3, ...: a fresh, larger stamp on every call.
    std::uint64_t nextStamp() { return ++clock_; }

    // Index of the line with the smallest stamp (the one stamped longest ago).
    static std::size_t lineWithSmallestStamp(const std::vector<CacheLine>& set);

private:
    std::uint64_t clock_ = 0;
};
