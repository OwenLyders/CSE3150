#include "core/Simulator.h"

#include <stdexcept>
#include <string>
#include <utility>

Simulator::Simulator(const CacheConfig& config, std::vector<MemRequest> trace, std::vector<PolicyKind> policies)
    : config_(config), trace_(std::move(trace)), policies_(std::move(policies)) {
    config_.validate();
    if (policies_.empty()) throw std::invalid_argument("the simulator needs at least one policy");
    for (std::size_t i = 0; i < trace_.size(); ++i) {
        if (trace_[i].address >= config_.memorySize()) {
            throw std::invalid_argument("request " + std::to_string(i + 1) + " (" + toString(trace_[i]) +
                                        ") is outside the " + std::to_string(config_.addressBits) +
                                        "-bit address space");
        }
    }
    buildCaches();
}

void Simulator::buildCaches() {
    caches_.clear();
    caches_.reserve(policies_.size());
    for (PolicyKind kind : policies_) caches_.emplace_back(config_, makePolicy(kind));
    history_.clear();
}

std::vector<AccessResult> Simulator::step() {
    if (finished()) throw std::logic_error("Simulator::step() called after the last request");

    const MemRequest& request = trace_[history_.size()];
    std::vector<AccessResult> results;
    results.reserve(caches_.size());
    for (Cache& cache : caches_) results.push_back(cache.access(request));
    history_.push_back(results);
    return results;
}

void Simulator::run() {
    while (!finished()) step();
}

void Simulator::reset() {
    buildCaches();
}

bool Simulator::finished() const {
    return history_.size() >= trace_.size();
}

std::size_t Simulator::position() const {
    return history_.size();
}
