#pragma once

#include "core/AccessResult.h"
#include "core/CacheConfig.h"
#include "core/CacheLine.h"
#include "core/CacheStats.h"
#include "core/MainMemory.h"
#include "core/MemRequest.h"
#include "core/ReplacementPolicy.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// A set-associative cache in front of its own copy of main memory.
//
// Write-back and write-allocate: a write changes only the cached byte and marks the line
// dirty, and memory sees the change when that line is evicted. A write miss loads the
// block first, just like a read miss.
class Cache {
public:
    // Throws std::invalid_argument if the config is invalid or there is no policy.
    Cache(const CacheConfig& config, std::unique_ptr<ReplacementPolicy> policy);

    // Handles one read or write and reports what happened. Throws std::out_of_range,
    // and changes nothing, if the address is outside main memory.
    AccessResult access(const MemRequest& request);

    const CacheConfig& config() const { return config_; }
    const CacheStats& stats() const { return stats_; }
    std::string policyName() const { return policy_->name(); }
    const MainMemory& memory() const { return memory_; }

    std::size_t numSets() const { return sets_.size(); }
    const std::vector<CacheLine>& set(std::size_t index) const { return sets_.at(index); }

    // The line the policy would evict on the next miss in this set, or nothing while
    // the set still has a free line.
    std::optional<std::size_t> nextVictim(std::size_t setIndex) const;

private:
    CacheConfig config_;
    std::unique_ptr<ReplacementPolicy> policy_;
    MainMemory memory_;
    std::vector<std::vector<CacheLine>> sets_;  // sets_[index][way]
    CacheStats stats_;
};
