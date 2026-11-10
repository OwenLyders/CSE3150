#include "TestFramework.h"

#include "core/Cache.h"
#include "core/PolicyFactory.h"
#include "core/TraceGenerator.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

CacheStats runThrough(PolicyKind kind, const CacheConfig& config, const std::vector<MemRequest>& trace) {
    Cache cache(config, makePolicy(kind));
    for (const MemRequest& request : trace) cache.access(request);
    return cache.stats();
}

std::size_t writesIn(const std::vector<MemRequest>& trace) {
    return static_cast<std::size_t>(std::count_if(trace.begin(), trace.end(), [](const MemRequest& request) {
        return request.op == Op::Write;
    }));
}

const PolicyKind kBothPolicies[] = {PolicyKind::Fifo, PolicyKind::Lru};

}  // namespace

TEST(sequential_reads_consecutive_addresses) {
    const CacheConfig config;
    const std::vector<MemRequest> trace = TraceGenerator(config).sequential();
    CHECK_EQ(trace.size(), 2 * config.cacheSize);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        CHECK(trace[i] == MemRequest::read(static_cast<Address>(i)));
    }
}

TEST(sequential_misses_once_per_block) {
    const CacheConfig config;
    const std::vector<MemRequest> trace = TraceGenerator(config).sequential();
    for (PolicyKind kind : kBothPolicies) {
        CHECK_EQ(runThrough(kind, config, trace).misses, trace.size() / config.blockSize);
    }
}

TEST(small_loop_misses_only_while_warming_up) {
    const CacheConfig config;
    const std::vector<MemRequest> trace = TraceGenerator(config).smallLoop();
    for (PolicyKind kind : kBothPolicies) {
        const CacheStats stats = runThrough(kind, config, trace);
        CHECK_EQ(stats.misses, config.numBlocks());
        CHECK_EQ(stats.evictions, 0);
    }
}

TEST(oversized_loop_uses_one_more_block_than_a_set_holds) {
    for (std::size_t ways : {1u, 2u, 4u}) {
        CacheConfig config;
        config.associativity = ways;
        const std::vector<MemRequest> trace = TraceGenerator(config).oversizedLoop();

        std::set<std::size_t> blocks;
        std::set<std::size_t> sets;
        for (const MemRequest& request : trace) {
            blocks.insert(request.address / config.blockSize);
            sets.insert(config.split(request.address).index);
        }
        CHECK_EQ(blocks.size(), ways + 1);
        CHECK_EQ(sets.size(), 1);
        for (PolicyKind kind : kBothPolicies) {
            CHECK_EQ(runThrough(kind, config, trace).hits, 0);
        }
    }
}

TEST(hot_cold_reuses_hot_blocks_and_reads_cold_blocks_once) {
    const CacheConfig config;
    const std::vector<MemRequest> trace = TraceGenerator(config).hotCold();
    CHECK_EQ(trace.size(), 200);

    const std::size_t hotBlocks = config.numSets() * (config.associativity / 2);
    std::map<std::size_t, std::size_t> usesPerBlock;
    for (const MemRequest& request : trace) ++usesPerBlock[request.address / config.blockSize];

    std::size_t hotRequests = 0;
    for (const auto& [block, uses] : usesPerBlock) {
        if (block < hotBlocks) {
            hotRequests += uses;
        } else {
            CHECK_EQ(uses, 1);  // a cold block is used once
        }
    }
    CHECK(hotRequests > trace.size() / 2);
}

TEST(hot_cold_and_random_include_writes) {
    const TraceGenerator generator{CacheConfig{}};
    CHECK(writesIn(generator.hotCold()) > 0);
    CHECK(writesIn(generator.random()) > 0);
}

TEST(random_stays_within_four_times_the_cache) {
    const CacheConfig config;
    for (const MemRequest& request : TraceGenerator(config).random()) {
        CHECK(request.address < 4 * config.cacheSize);
    }
}

TEST(same_seed_gives_the_same_trace) {
    const CacheConfig config;
    CHECK(TraceGenerator(config, 7).random() == TraceGenerator(config, 7).random());
    CHECK(TraceGenerator(config, 7).hotCold() == TraceGenerator(config, 7).hotCold());
    CHECK(TraceGenerator(config, 7).random() != TraceGenerator(config, 8).random());
}

TEST(every_builtin_fits_in_a_small_memory) {
    CacheConfig config;
    config.addressBits = 8;  // 256 bytes of memory behind a 64-byte cache
    for (const NamedTrace& trace : TraceGenerator(config).builtins()) {
        for (const MemRequest& request : trace.requests) CHECK(request.address < config.memorySize());
    }
}

// The smallest memory allowed (twice the cache) must not change what the traces do.
TEST(builtins_keep_their_shape_in_the_smallest_memory) {
    CacheConfig config;
    config.addressBits = 7;  // 128 bytes of memory behind a 64-byte cache
    const TraceGenerator generator(config);
    for (const NamedTrace& trace : generator.builtins()) {
        for (const MemRequest& request : trace.requests) CHECK(request.address < config.memorySize());
    }

    std::set<std::size_t> loopBlocks;
    for (const MemRequest& request : generator.oversizedLoop()) loopBlocks.insert(request.address / config.blockSize);
    CHECK_EQ(loopBlocks.size(), config.associativity + 1);
    for (PolicyKind kind : kBothPolicies) {
        CHECK_EQ(runThrough(kind, config, generator.oversizedLoop()).hits, 0);
        CHECK_EQ(runThrough(kind, config, generator.sequential()).misses, 2 * config.numBlocks());
    }

    // Cold blocks never land on hot blocks, even when memory is too small to use each one
    // only once. The same seed makes the same hot/cold choices whatever the memory size, so
    // a roomy memory shows which requests are the cold ones.
    CacheConfig roomy = config;
    roomy.addressBits = 16;
    const std::size_t hotBlocks = config.numSets() * (config.associativity / 2);
    const std::vector<MemRequest> small = generator.hotCold();
    const std::vector<MemRequest> large = TraceGenerator(roomy).hotCold();
    CHECK_EQ(small.size(), large.size());
    for (std::size_t i = 0; i < small.size() && i < large.size(); ++i) {
        const bool cold = large[i].address / config.blockSize >= hotBlocks;
        if (cold) CHECK(small[i].address / config.blockSize >= hotBlocks);
    }
}

// hot-cold and random have to get past warm-up: 25 requests per hot block and 12 per cache
// block (200 each with the default cache), capped at 20,000.
TEST(hot_cold_and_random_grow_with_the_cache) {
    CacheConfig config;
    CHECK_EQ(TraceGenerator(config).hotCold().size(), 200);
    CHECK_EQ(TraceGenerator(config).random().size(), 200);

    config.cacheSize = 1024;  // 256 blocks, 128 of them hot
    CHECK_EQ(TraceGenerator(config).hotCold().size(), 25 * 128);
    CHECK_EQ(TraceGenerator(config).random().size(), 12 * 256);

    config.cacheSize = 65536;
    config.addressBits = 17;  // memory must be at least twice the cache
    CHECK_EQ(TraceGenerator(config).hotCold().size(), 20000);
    CHECK_EQ(TraceGenerator(config).random().size(), 20000);
}

TEST(builtins_are_the_five_from_the_design_plan) {
    const std::vector<std::string> expected{"sequential", "small-loop", "oversized-loop", "hot-cold", "random"};
    CHECK_EQ(TraceGenerator::builtinNames(), expected);
    for (const NamedTrace& trace : TraceGenerator(CacheConfig{}).builtins()) {
        CHECK(!trace.description.empty());
        CHECK(!trace.requests.empty());
    }
}

TEST(finds_a_builtin_by_name) {
    const TraceGenerator generator{CacheConfig{}};
    const std::optional<NamedTrace> hotCold = generator.builtin("hot-cold");
    CHECK(hotCold.has_value());
    CHECK(hotCold && hotCold->requests == generator.hotCold());
    CHECK(!generator.builtin("nope"));
}
