#include "core/Cache.h"

#include "core/Format.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace {

const CacheConfig& validated(const CacheConfig& config) {
    config.validate();
    return config;
}

CacheLine emptyLine(std::size_t blockSize) {
    CacheLine line;
    line.data.assign(blockSize, 0);
    return line;
}

// The way of the valid line holding `tag`, if there is one.
std::optional<std::size_t> findTag(const std::vector<CacheLine>& set, std::uint32_t tag) {
    for (std::size_t way = 0; way < set.size(); ++way) {
        if (set[way].valid && set[way].tag == tag) return way;
    }
    return std::nullopt;
}

// The way of the first line that holds no block, if there is one.
std::optional<std::size_t> findFreeLine(const std::vector<CacheLine>& set) {
    for (std::size_t way = 0; way < set.size(); ++way) {
        if (!set[way].valid) return way;
    }
    return std::nullopt;
}

}  // namespace

Cache::Cache(const CacheConfig& config, std::unique_ptr<ReplacementPolicy> policy)
    : config_(validated(config)),
      policy_(std::move(policy)),
      memory_(config_.memorySize()),
      sets_(config_.numSets(), std::vector<CacheLine>(config_.associativity, emptyLine(config_.blockSize))) {
    if (!policy_) throw std::invalid_argument("a cache needs a replacement policy");
}

AccessResult Cache::access(const MemRequest& request) {
    if (request.address >= config_.memorySize()) {
        throw std::out_of_range("address " + hex(request.address) + " is outside the " +
                                std::to_string(config_.addressBits) + "-bit address space");
    }

    // 1. Split the address into tag, set index and offset.
    const AddressParts parts = config_.split(request.address);
    std::vector<CacheLine>& set = sets_[parts.index];

    AccessResult result;
    result.setIndex = parts.index;
    result.tag = parts.tag;
    result.offset = parts.offset;

    // 2. Search that set for a valid line with a matching tag.
    std::optional<std::size_t> way = findTag(set, parts.tag);
    result.hit = way.has_value();

    if (result.hit) {
        // 3. A hit: tell the policy (LRU re-stamps the line, FIFO does nothing).
        policy_->onHit(set[*way]);
    } else {
        // 4. A miss: use a free line if the set has one...
        way = findFreeLine(set);

        // 5. ...otherwise the policy picks a victim, written back to memory first if dirty.
        if (!way) {
            way = policy_->chooseVictim(set);
            if (*way >= set.size()) {
                throw std::logic_error(policy_->name() + " chose line " + std::to_string(*way) + " of a " +
                                       std::to_string(set.size()) + "-line set");
            }
            const CacheLine& victim = set[*way];
            result.evictedTag = victim.tag;
            ++stats_.evictions;
            if (victim.dirty) {
                memory_.writeBlock(config_.blockAddress(victim.tag, parts.index), victim.data);
                result.wroteBack = true;
                ++stats_.writeBacks;
            }
        }

        // Load the new block from main memory into the chosen line.
        CacheLine& line = set[*way];
        line.valid = true;
        line.dirty = false;
        line.tag = parts.tag;
        line.data = memory_.readBlock(config_.blockAddress(parts.tag, parts.index), config_.blockSize);
        policy_->onLoad(line);
    }

    // Now the block is in the line, so read or write the byte at the offset.
    // A write marks the line dirty.
    CacheLine& line = set[*way];
    if (request.op == Op::Write) {
        line.data[parts.offset] = request.value;
        line.dirty = true;
    }
    result.way = *way;
    result.value = line.data[parts.offset];

    // 6. Update the counters.
    ++stats_.accesses;
    if (result.hit) {
        ++stats_.hits;
    } else {
        ++stats_.misses;
    }
    return result;
}

std::optional<std::size_t> Cache::nextVictim(std::size_t setIndex) const {
    const std::vector<CacheLine>& set = sets_.at(setIndex);
    if (findFreeLine(set)) return std::nullopt;
    return policy_->chooseVictim(set);
}
