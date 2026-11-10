#include "TestFramework.h"

#include "core/Cache.h"
#include "core/PolicyFactory.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

Cache makeCache(PolicyKind kind, const CacheConfig& config = CacheConfig{}) {
    return Cache(config, makePolicy(kind));
}

// With the default config (4 sets of 4-byte blocks), blocks 16 bytes apart share a set,
// so these five blocks all land in set 0.
const Address A = 0x000;
const Address B = 0x010;
const Address C = 0x020;
const Address D = 0x030;
const Address E = 0x040;

// Reads each address in turn; returns 'h' or 'm' for each.
std::string readAll(Cache& cache, const std::vector<Address>& addresses) {
    std::string outcomes;
    for (Address address : addresses) {
        outcomes += cache.access(MemRequest::read(address)).hit ? 'h' : 'm';
    }
    return outcomes;
}

// One set of `ways` lines holding 4-byte blocks (a fully associative cache).
CacheConfig oneSet(std::size_t ways) {
    CacheConfig config;
    config.blockSize = 4;
    config.associativity = ways;
    config.cacheSize = 4 * ways;
    return config;
}

}  // namespace

TEST(first_access_misses_second_hits) {
    Cache cache = makeCache(PolicyKind::Fifo);
    const AccessResult first = cache.access(MemRequest::read(0x1A2E));
    CHECK(!first.hit);
    CHECK_EQ(first.setIndex, 3);
    CHECK_EQ(first.tag, 0x1A2);
    CHECK_EQ(first.offset, 2);
    CHECK(!first.evictedTag);

    const AccessResult second = cache.access(MemRequest::read(0x1A2E));
    CHECK(second.hit);
    CHECK_EQ(second.way, first.way);

    CHECK_EQ(cache.stats().accesses, 2);
    CHECK_EQ(cache.stats().hits, 1);
    CHECK_EQ(cache.stats().misses, 1);
}

TEST(neighbor_in_same_block_hits) {
    Cache cache = makeCache(PolicyKind::Fifo);
    cache.access(MemRequest::read(0x1A2C));
    CHECK(cache.access(MemRequest::read(0x1A2F)).hit);
}

TEST(miss_loads_the_whole_block_from_memory) {
    Cache cache = makeCache(PolicyKind::Fifo);
    const AccessResult result = cache.access(MemRequest::read(0x1A2E));
    CHECK_EQ(result.value, 0x2E);

    const CacheLine& line = cache.set(3)[result.way];
    CHECK(line.valid);
    CHECK(!line.dirty);
    CHECK_EQ(line.tag, 0x1A2);
    const std::vector<std::uint8_t> expected{0x2C, 0x2D, 0x2E, 0x2F};
    CHECK_EQ(line.data, expected);
}

TEST(write_changes_cached_byte_and_marks_line_dirty) {
    Cache cache = makeCache(PolicyKind::Fifo);
    const AccessResult write = cache.access(MemRequest::write(0x1A2E, 0x7F));
    CHECK(!write.hit);  // write-allocate: the miss loads the block, then writes into it
    CHECK_EQ(write.value, 0x7F);
    const CacheLine& line = cache.set(3)[write.way];
    CHECK(line.dirty);
    CHECK_EQ(line.data[2], 0x7F);

    CHECK_EQ(cache.access(MemRequest::read(0x1A2E)).value, 0x7F);
    CHECK_EQ(cache.memory().readByte(0x1A2E), 0x2E);  // write-back: memory waits for the eviction
}

TEST(fills_free_lines_before_evicting) {
    Cache cache = makeCache(PolicyKind::Fifo);
    std::vector<std::size_t> ways;
    for (Address address : {A, B, C, D}) {
        const AccessResult result = cache.access(MemRequest::read(address));
        CHECK(!result.evictedTag);
        ways.push_back(result.way);
    }
    CHECK_EQ(ways, (std::vector<std::size_t>{0, 1, 2, 3}));
    CHECK_EQ(cache.stats().evictions, 0);
}

// The design plan's worked example, grown by one block to fit a real 4-way set: A B C D A E A.
TEST(fifo_evicts_the_block_loaded_first) {
    Cache cache = makeCache(PolicyKind::Fifo);
    CHECK_EQ(readAll(cache, {A, B, C, D, A}), "mmmmh");

    const AccessResult e = cache.access(MemRequest::read(E));
    CHECK_EQ(e.evictedTag.value_or(999), cache.config().split(A).tag);

    const AccessResult a = cache.access(MemRequest::read(A));
    CHECK(!a.hit);
    CHECK_EQ(a.evictedTag.value_or(999), cache.config().split(B).tag);
    CHECK_EQ(cache.stats().hits, 1);
}

TEST(lru_evicts_the_block_used_least_recently) {
    Cache cache = makeCache(PolicyKind::Lru);
    CHECK_EQ(readAll(cache, {A, B, C, D, A}), "mmmmh");

    const AccessResult e = cache.access(MemRequest::read(E));
    CHECK_EQ(e.evictedTag.value_or(999), cache.config().split(B).tag);

    CHECK(cache.access(MemRequest::read(A)).hit);
    CHECK_EQ(cache.stats().hits, 2);
}

// A B A C B on a single set of 2 lines.
TEST(fifo_beats_lru_through_a_real_cache) {
    Cache fifo = makeCache(PolicyKind::Fifo, oneSet(2));
    Cache lru = makeCache(PolicyKind::Lru, oneSet(2));
    CHECK_EQ(readAll(fifo, {0x00, 0x04, 0x00, 0x08, 0x04}), "mmhmh");
    CHECK_EQ(readAll(lru, {0x00, 0x04, 0x00, 0x08, 0x04}), "mmhmm");
}

TEST(evicting_a_dirty_block_writes_it_back) {
    Cache cache = makeCache(PolicyKind::Fifo, oneSet(2));
    cache.access(MemRequest::write(0x01, 0x7F));  // block 0x00 is now dirty
    cache.access(MemRequest::read(0x04));

    const AccessResult eviction = cache.access(MemRequest::read(0x08));  // set full: block 0x00 goes
    CHECK(eviction.evictedTag.has_value());
    CHECK(eviction.wroteBack);
    CHECK_EQ(cache.stats().writeBacks, 1);
    CHECK_EQ(cache.memory().readByte(0x01), 0x7F);

    const AccessResult reload = cache.access(MemRequest::read(0x01));  // comes back from memory
    CHECK(!reload.hit);
    CHECK_EQ(reload.value, 0x7F);
}

TEST(evicting_a_clean_block_does_not_write_back) {
    Cache cache = makeCache(PolicyKind::Fifo, oneSet(2));
    readAll(cache, {0x00, 0x04});
    const AccessResult eviction = cache.access(MemRequest::read(0x08));
    CHECK(eviction.evictedTag.has_value());
    CHECK(!eviction.wroteBack);
    CHECK_EQ(cache.stats().evictions, 1);
    CHECK_EQ(cache.stats().writeBacks, 0);
}

TEST(next_victim_is_empty_until_the_set_is_full) {
    Cache cache = makeCache(PolicyKind::Fifo);
    readAll(cache, {A, B, C});
    CHECK(!cache.nextVictim(0));
    cache.access(MemRequest::read(D));
    CHECK_EQ(cache.nextVictim(0).value_or(99), 0);  // A's line, loaded first
}

TEST(next_victim_shows_the_policies_disagree) {
    Cache fifo = makeCache(PolicyKind::Fifo);
    Cache lru = makeCache(PolicyKind::Lru);
    readAll(fifo, {A, B, C, D, A});
    readAll(lru, {A, B, C, D, A});
    CHECK_EQ(fifo.nextVictim(0).value_or(99), 0);  // A's line
    CHECK_EQ(lru.nextVictim(0).value_or(99), 1);   // B's line
}

TEST(address_outside_memory_throws_and_changes_nothing) {
    Cache cache = makeCache(PolicyKind::Fifo);
    CHECK_THROWS(cache.access(MemRequest::read(0x10000)), std::out_of_range);
    CHECK_EQ(cache.stats().accesses, 0);
}

TEST(constructor_rejects_bad_config_or_missing_policy) {
    CacheConfig bad;
    bad.blockSize = 3;
    CHECK_THROWS(Cache(bad, makePolicy(PolicyKind::Fifo)), std::invalid_argument);
    CHECK_THROWS(Cache(CacheConfig{}, nullptr), std::invalid_argument);
}

// A policy that names a line outside the set (say, a buggy new policy) must be caught,
// not allowed to write past the end of the set.
TEST(rejects_a_victim_outside_the_set) {
    class BrokenPolicy : public ReplacementPolicy {
    public:
        std::string name() const override { return "BROKEN"; }
        void onLoad(CacheLine&) override {}
        void onHit(CacheLine&) override {}
        std::size_t chooseVictim(const std::vector<CacheLine>& set) const override { return set.size(); }
    };
    Cache cache(oneSet(2), std::make_unique<BrokenPolicy>());
    readAll(cache, {0x00, 0x04});
    CHECK_THROWS(cache.access(MemRequest::read(0x08)), std::logic_error);
    CHECK_EQ(cache.stats().evictions, 0);
}

TEST(policy_name_comes_from_the_policy) {
    CHECK_EQ(makeCache(PolicyKind::Fifo).policyName(), "FIFO");
    CHECK_EQ(makeCache(PolicyKind::Lru).policyName(), "LRU");
}
