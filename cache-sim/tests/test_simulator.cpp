#include "TestFramework.h"

#include "core/Simulator.h"
#include "core/TraceGenerator.h"
#include "core/TraceReader.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<MemRequest> reads(const std::vector<Address>& addresses) {
    std::vector<MemRequest> trace;
    for (Address address : addresses) trace.push_back(MemRequest::read(address));
    return trace;
}

std::vector<MemRequest> sampleTrace(const std::string& name) {
    return TraceReader::load(std::string(CACHESIM_TRACE_DIR) + "/" + name, 16);
}

}  // namespace

TEST(runs_fifo_then_lru_by_default) {
    const Simulator sim(CacheConfig{}, reads({0x10}));
    CHECK_EQ(sim.cacheCount(), 2);
    CHECK_EQ(sim.cache(0).policyName(), "FIFO");
    CHECK_EQ(sim.cache(1).policyName(), "LRU");
}

TEST(step_sends_the_same_request_to_every_cache) {
    Simulator sim(CacheConfig{}, reads({0x1A2E, 0x1A2E}));
    const std::vector<AccessResult> first = sim.step();
    CHECK_EQ(first.size(), 2);
    CHECK(!first[0].hit);
    CHECK(!first[1].hit);
    CHECK_EQ(first[0].setIndex, 3);
    CHECK_EQ(first[1].setIndex, 3);
    CHECK_EQ(sim.position(), 1);
    CHECK(!sim.finished());

    const std::vector<AccessResult> second = sim.step();
    CHECK(second[0].hit);
    CHECK(second[1].hit);
    CHECK(sim.finished());
}

TEST(history_keeps_every_step) {
    Simulator sim(CacheConfig{}, reads({0x00, 0x00, 0x10}));
    sim.run();
    CHECK_EQ(sim.history().size(), 3);
    CHECK(!sim.history()[0][0].hit);
    CHECK(sim.history()[1][0].hit);
    CHECK(sim.history()[1][1].hit);
}

// The sample traces state their expected results in comments; check that those are true.
TEST(lru_wins_sample_trace) {
    Simulator sim(CacheConfig{}, sampleTrace("lru-wins.trace"));
    sim.run();
    CHECK_EQ(sim.cache(0).stats().hits, 1);  // FIFO
    CHECK_EQ(sim.cache(1).stats().hits, 2);  // LRU
}

TEST(fifo_wins_sample_trace) {
    Simulator sim(CacheConfig{}, sampleTrace("fifo-wins.trace"));
    sim.run();
    CHECK_EQ(sim.cache(0).stats().hits, 4);  // FIFO
    CHECK_EQ(sim.cache(1).stats().hits, 1);  // LRU
}

TEST(write_back_sample_trace) {
    Simulator sim(CacheConfig{}, sampleTrace("write-back.trace"));
    sim.run();
    for (std::size_t c = 0; c < sim.cacheCount(); ++c) {
        CHECK_EQ(sim.cache(c).stats().writeBacks, 1);
        CHECK_EQ(sim.cache(c).memory().readByte(0x0001), 0x7F);
        CHECK_EQ(sim.history().back()[c].value, 0x7F);
    }
}

// From the design plan: hits plus misses equals accesses after every trace.
TEST(counters_add_up_on_every_builtin_trace) {
    const CacheConfig config;
    for (const NamedTrace& trace : TraceGenerator(config).builtins()) {
        Simulator sim(config, trace.requests);
        sim.run();
        for (std::size_t c = 0; c < sim.cacheCount(); ++c) {
            const CacheStats& stats = sim.cache(c).stats();
            CHECK_EQ(stats.accesses, trace.requests.size());
            CHECK_EQ(stats.hits + stats.misses, stats.accesses);
            CHECK(stats.evictions <= stats.misses);
            CHECK(stats.writeBacks <= stats.evictions);
        }
    }
}

// With 1 way a block has only one possible line, so the policy never gets a choice.
TEST(direct_mapped_fifo_and_lru_are_identical) {
    CacheConfig config;
    config.associativity = 1;
    Simulator sim(config, TraceGenerator(config).random());
    sim.run();
    for (const std::vector<AccessResult>& step : sim.history()) {
        CHECK_EQ(step[0].hit, step[1].hit);
        CHECK_EQ(step[0].way, step[1].way);
        CHECK(step[0].evictedTag == step[1].evictedTag);
    }
    CHECK_EQ(sim.cache(0).stats().hits, sim.cache(1).stats().hits);
    CHECK_EQ(sim.cache(0).stats().writeBacks, sim.cache(1).stats().writeBacks);
}

// One policy's write-backs must never change what the other policy loads.
TEST(each_cache_has_its_own_memory) {
    CacheConfig config;  // a single set of 2 lines
    config.cacheSize = 8;
    config.associativity = 2;
    // A B A C: FIFO evicts the dirty A, LRU evicts the clean B.
    const std::vector<MemRequest> trace{MemRequest::write(0x00, 0x7F), MemRequest::read(0x04),
                                        MemRequest::read(0x00), MemRequest::read(0x08)};
    Simulator sim(config, trace);
    sim.run();
    CHECK(sim.history()[3][0].wroteBack);
    CHECK(!sim.history()[3][1].wroteBack);
    CHECK_EQ(sim.cache(0).memory().readByte(0x00), 0x7F);
    CHECK_EQ(sim.cache(1).memory().readByte(0x00), 0x00);
}

TEST(reset_starts_over_and_replays_identically) {
    const CacheConfig config;
    Simulator sim(config, TraceGenerator(config).hotCold());
    sim.run();
    const CacheStats fifoBefore = sim.cache(0).stats();
    const CacheStats lruBefore = sim.cache(1).stats();

    sim.reset();
    CHECK_EQ(sim.position(), 0);
    CHECK(sim.history().empty());
    CHECK_EQ(sim.cache(0).stats().accesses, 0);
    for (std::size_t s = 0; s < sim.cache(0).numSets(); ++s) {
        for (const CacheLine& line : sim.cache(0).set(s)) CHECK(!line.valid);
    }

    sim.run();
    CHECK_EQ(sim.cache(0).stats().hits, fifoBefore.hits);
    CHECK_EQ(sim.cache(1).stats().hits, lruBefore.hits);
    CHECK_EQ(sim.cache(0).stats().writeBacks, fifoBefore.writeBacks);
    CHECK_EQ(sim.cache(1).stats().writeBacks, lruBefore.writeBacks);
}

TEST(step_after_the_end_throws) {
    Simulator sim(CacheConfig{}, reads({0x10}));
    sim.step();
    CHECK(sim.finished());
    CHECK_THROWS(sim.step(), std::logic_error);
}

TEST(empty_trace_is_finished_from_the_start) {
    const Simulator sim(CacheConfig{}, {});
    CHECK(sim.finished());
    CHECK_EQ(sim.position(), 0);
}

TEST(rejects_a_request_outside_memory) {
    const std::string message =
        testing::thrownMessage([] { (void)Simulator(CacheConfig{}, reads({0x10, 0x10000})); });
    CHECK(message.find("request 2") != std::string::npos);
}

TEST(rejects_invalid_config_and_empty_policy_list) {
    CacheConfig bad;
    bad.associativity = 3;
    CHECK_THROWS(Simulator(bad, reads({0x10})), std::invalid_argument);
    CHECK_THROWS(Simulator(CacheConfig{}, reads({0x10}), {}), std::invalid_argument);
}

TEST(can_run_a_single_policy) {
    Simulator sim(CacheConfig{}, reads({0x10}), {PolicyKind::Lru});
    CHECK_EQ(sim.cacheCount(), 1);
    CHECK_EQ(sim.step().size(), 1);
}
