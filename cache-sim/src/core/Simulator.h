#pragma once

#include "core/AccessResult.h"
#include "core/Cache.h"
#include "core/CacheConfig.h"
#include "core/MemRequest.h"
#include "core/PolicyFactory.h"

#include <cstddef>
#include <vector>

// Replays one trace through one cache per policy, advancing them together so every policy
// sees exactly the same requests. Each cache has its own copy of main memory.
//
// This is the interface the GUI and the command-line runner share. Neither changes the
// simulation except through step(), run() and reset().
class Simulator {
public:
    // Throws std::invalid_argument for an invalid config, an empty policy list, or a request
    // whose address does not fit in the address width.
    Simulator(const CacheConfig& config, std::vector<MemRequest> trace,
              std::vector<PolicyKind> policies = {PolicyKind::Fifo, PolicyKind::Lru});

    // Sends the next request to every cache and returns one result per cache, in policy order.
    // Throws std::logic_error if the trace is already finished.
    std::vector<AccessResult> step();
    void run();    // steps until finished
    void reset();  // back to the first request, with empty caches and fresh memory

    bool finished() const;
    std::size_t position() const;  // how many requests have been processed
    const std::vector<MemRequest>& trace() const { return trace_; }
    const CacheConfig& config() const { return config_; }

    std::size_t cacheCount() const { return caches_.size(); }
    const Cache& cache(std::size_t index) const { return caches_.at(index); }

    // history()[i][c] is what request i did in cache c, for every processed request.
    const std::vector<std::vector<AccessResult>>& history() const { return history_; }

private:
    void buildCaches();

    CacheConfig config_;
    std::vector<MemRequest> trace_;
    std::vector<PolicyKind> policies_;
    std::vector<Cache> caches_;
    std::vector<std::vector<AccessResult>> history_;
};
