#pragma once

#include "core/CacheConfig.h"
#include "core/MemRequest.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// A trace plus the name and one-line description that menus and reports show.
struct NamedTrace {
    std::string name;  // what --builtin takes, e.g. "hot-cold"
    std::string description;
    std::vector<MemRequest> requests;
};

// Builds the five synthetic traces from the design plan. Each one is sized from the cache
// config, so it shows the same behavior whatever settings are chosen, and every address
// stays inside main memory. The random ones use a fixed seed and std::mt19937, whose output
// is the same on every platform.
class TraceGenerator {
public:
    static constexpr std::uint32_t kDefaultSeed = 3150;

    explicit TraceGenerator(const CacheConfig& config, std::uint32_t seed = kDefaultSeed);

    // Reads addresses 0, 1, 2, ... for twice the cache size.
    std::vector<MemRequest> sequential() const;

    // Loops `passes` times over exactly as many blocks as the cache holds.
    std::vector<MemRequest> smallLoop(std::size_t passes = 8) const;

    // Loops `passes` times over associativity + 1 blocks that all map to set 0.
    std::vector<MemRequest> oversizedLoop(std::size_t passes = 16) const;

    // Hot blocks (half the cache) get 3 of every 4 requests, a quarter of those writes.
    // The rest read cold blocks, each used once while memory has room for that.
    // 25 requests per hot block (200 with the default cache), at most 20,000.
    std::vector<MemRequest> hotCold() const;

    // Uniform random addresses over 4x the cache size, a quarter of them writes.
    // (Over the whole 64 KiB address space nearly every request would miss.)
    // 12 requests per cache block (200 with the default cache), at most 20,000.
    std::vector<MemRequest> random() const;

    // All five, in the order the GUI's T key cycles through them.
    std::vector<NamedTrace> builtins() const;
    std::optional<NamedTrace> builtin(const std::string& name) const;
    static std::vector<std::string> builtinNames();

private:
    std::vector<MemRequest> generate(const std::string& name) const;

    CacheConfig config_;
    std::uint32_t seed_;
};
