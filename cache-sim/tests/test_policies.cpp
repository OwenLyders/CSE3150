#include "TestFramework.h"

#include "core/CacheLine.h"
#include "core/FifoPolicy.h"
#include "core/LruPolicy.h"
#include "core/PolicyFactory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

// What happened when a string of block names went through one set.
struct SetRun {
    int hits = 0;
    std::string outcomes;  // one letter per request: h = hit, m = miss
    std::string evicted;   // the evicted blocks, in order
};

// Sends the blocks (one letter each) through a single set of `lines` lines using only the
// policy, following the same hit / free line / victim steps as Cache::access. This lets us
// check the design plan's 3-line example, which no power-of-two cache size can express.
SetRun runOneSet(ReplacementPolicy& policy, std::size_t lines, const std::string& blocks) {
    std::vector<CacheLine> set(lines);
    SetRun run;
    for (char block : blocks) {
        const auto tag = static_cast<std::uint32_t>(block);

        CacheLine* line = nullptr;
        for (CacheLine& candidate : set) {
            if (candidate.valid && candidate.tag == tag) line = &candidate;
        }
        if (line) {
            policy.onHit(*line);
            ++run.hits;
            run.outcomes += 'h';
            continue;
        }

        for (CacheLine& candidate : set) {
            if (!line && !candidate.valid) line = &candidate;
        }
        if (!line) {
            line = &set[policy.chooseVictim(set)];
            run.evicted += static_cast<char>(line->tag);
        }
        line->valid = true;
        line->tag = tag;
        policy.onLoad(*line);
        run.outcomes += 'm';
    }
    return run;
}

}  // namespace

// The worked example from the design plan: one set of 3 lines, requests A B C A D A.
TEST(fifo_worked_example_on_three_lines) {
    FifoPolicy fifo;
    const SetRun run = runOneSet(fifo, 3, "ABCADA");
    CHECK_EQ(run.outcomes, "mmmhmm");
    CHECK_EQ(run.hits, 1);
    CHECK_EQ(run.evicted, "AB");
}

TEST(lru_worked_example_on_three_lines) {
    LruPolicy lru;
    const SetRun run = runOneSet(lru, 3, "ABCADA");
    CHECK_EQ(run.outcomes, "mmmhmh");
    CHECK_EQ(run.hits, 2);
    CHECK_EQ(run.evicted, "B");
}

// LRU does not always win: A B A C B on 2 lines.
TEST(fifo_beats_lru_on_two_lines) {
    FifoPolicy fifo;
    LruPolicy lru;
    const SetRun fifoRun = runOneSet(fifo, 2, "ABACB");
    const SetRun lruRun = runOneSet(lru, 2, "ABACB");
    CHECK_EQ(fifoRun.hits, 2);
    CHECK_EQ(lruRun.hits, 1);
    CHECK_EQ(fifoRun.evicted, "A");
    CHECK_EQ(lruRun.evicted, "BA");
}

TEST(fifo_stamps_on_load_only) {
    FifoPolicy fifo;
    std::vector<CacheLine> set(2);
    fifo.onLoad(set[0]);
    fifo.onLoad(set[1]);
    const std::uint64_t stampBeforeHit = set[0].stamp;
    fifo.onHit(set[0]);
    CHECK_EQ(set[0].stamp, stampBeforeHit);
    CHECK_EQ(fifo.chooseVictim(set), 0);  // loaded first
}

TEST(lru_restamps_on_hit) {
    LruPolicy lru;
    std::vector<CacheLine> set(2);
    lru.onLoad(set[0]);
    lru.onLoad(set[1]);
    lru.onHit(set[0]);
    CHECK(set[0].stamp > set[1].stamp);
    CHECK_EQ(lru.chooseVictim(set), 1);  // used least recently
}

TEST(choose_victim_is_repeatable) {
    LruPolicy lru;
    std::vector<CacheLine> set(3);
    for (CacheLine& line : set) lru.onLoad(line);
    CHECK_EQ(lru.chooseVictim(set), 0);
    CHECK_EQ(lru.chooseVictim(set), 0);  // asking again does not change the answer
}

TEST(factory_makes_each_policy) {
    CHECK_EQ(makePolicy(PolicyKind::Fifo)->name(), "FIFO");
    CHECK_EQ(makePolicy(PolicyKind::Lru)->name(), "LRU");
}
