#include "core/TraceGenerator.h"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace {

struct BuiltinInfo {
    const char* name;
    const char* description;
};

// The built-in traces, in the order the GUI's T key cycles through them.
const BuiltinInfo kBuiltins[] = {
    {"sequential", "Reads addresses 0, 1, 2, ... in order"},
    {"small-loop", "Repeats a set of blocks that fits in the cache"},
    {"oversized-loop", "Repeats one more block than a set can hold"},
    {"hot-cold", "A few blocks reused often, mixed with blocks used once"},
    {"random", "Uniform random addresses over 4x the cache size, fixed seed"},
};

// A number in [0, n). std::mt19937 produces the same sequence on every platform, but
// std::uniform_int_distribution does not, so we take the remainder ourselves. (The
// tiny bias this adds does not matter here.) Each call draws exactly once, so draw
// into named variables, one per statement, to keep the order fixed.
std::size_t below(std::mt19937& rng, std::size_t n) {
    return static_cast<std::size_t>(rng()) % n;
}

std::uint8_t randomByte(std::mt19937& rng) {
    return static_cast<std::uint8_t>(rng() & 0xFF);
}

// Long enough to get well past warm-up, without letting huge caches make huge traces.
std::size_t traceLength(std::size_t perUnit, std::size_t units) {
    return std::clamp<std::size_t>(perUnit * units, 200, 20000);
}

}  // namespace

// validate() guarantees memory is at least twice the cache, which is what keeps every
// address below inside main memory.
TraceGenerator::TraceGenerator(const CacheConfig& config, std::uint32_t seed) : config_(config), seed_(seed) {
    config_.validate();
}

std::vector<MemRequest> TraceGenerator::sequential() const {
    std::vector<MemRequest> trace;
    for (std::size_t byte = 0; byte < 2 * config_.cacheSize; ++byte) {
        trace.push_back(MemRequest::read(static_cast<Address>(byte)));
    }
    return trace;
}

std::vector<MemRequest> TraceGenerator::smallLoop(std::size_t passes) const {
    std::vector<MemRequest> trace;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t block = 0; block < config_.numBlocks(); ++block) {
            // Each pass reads a different byte of the block, which still hits.
            const std::size_t offset = pass % config_.blockSize;
            trace.push_back(MemRequest::read(static_cast<Address>(block * config_.blockSize + offset)));
        }
    }
    return trace;
}

std::vector<MemRequest> TraceGenerator::oversizedLoop(std::size_t passes) const {
    const std::size_t blocks = config_.associativity + 1;
    const std::size_t stride = config_.numSets() * config_.blockSize;  // blocks this far apart share a set
    std::vector<MemRequest> trace;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t block = 0; block < blocks; ++block) {
            trace.push_back(MemRequest::read(static_cast<Address>(block * stride)));
        }
    }
    return trace;
}

std::vector<MemRequest> TraceGenerator::hotCold() const {
    std::mt19937 rng(seed_);
    const std::size_t hotPerSet = std::max<std::size_t>(1, config_.associativity / 2);
    const std::size_t hotBlocks = config_.numSets() * hotPerSet;  // blocks 0 .. hotBlocks-1 are hot
    const std::size_t coldBlocks = config_.memorySize() / config_.blockSize - hotBlocks;  // all the rest
    const std::size_t count = traceLength(25, hotBlocks);

    std::vector<MemRequest> trace;
    std::size_t coldReads = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const bool hot = below(rng, 4) != 0;  // 3 in 4 requests
        if (!hot) {
            // The next cold block. Only a tiny memory runs out and starts reusing them.
            const std::size_t block = hotBlocks + coldReads % coldBlocks;
            trace.push_back(MemRequest::read(static_cast<Address>(block * config_.blockSize)));
            ++coldReads;
            continue;
        }
        const std::size_t block = below(rng, hotBlocks);
        const std::size_t offset = below(rng, config_.blockSize);
        const Address address = static_cast<Address>(block * config_.blockSize + offset);
        const bool isWrite = below(rng, 4) == 0;  // a quarter of the hot requests
        if (isWrite) {
            const std::uint8_t value = randomByte(rng);
            trace.push_back(MemRequest::write(address, value));
        } else {
            trace.push_back(MemRequest::read(address));
        }
    }
    return trace;
}

std::vector<MemRequest> TraceGenerator::random() const {
    std::mt19937 rng(seed_);
    const std::size_t range = std::min(config_.memorySize(), 4 * config_.cacheSize);
    const std::size_t count = traceLength(12, config_.numBlocks());

    std::vector<MemRequest> trace;
    for (std::size_t i = 0; i < count; ++i) {
        const Address address = static_cast<Address>(below(rng, range));
        const bool isWrite = below(rng, 4) == 0;
        if (isWrite) {
            const std::uint8_t value = randomByte(rng);
            trace.push_back(MemRequest::write(address, value));
        } else {
            trace.push_back(MemRequest::read(address));
        }
    }
    return trace;
}

std::vector<MemRequest> TraceGenerator::generate(const std::string& name) const {
    if (name == "sequential") return sequential();
    if (name == "small-loop") return smallLoop();
    if (name == "oversized-loop") return oversizedLoop();
    if (name == "hot-cold") return hotCold();
    if (name == "random") return random();
    throw std::invalid_argument("unknown built-in trace '" + name + "'");
}

std::vector<NamedTrace> TraceGenerator::builtins() const {
    std::vector<NamedTrace> traces;
    for (const BuiltinInfo& info : kBuiltins) traces.push_back({info.name, info.description, generate(info.name)});
    return traces;
}

std::optional<NamedTrace> TraceGenerator::builtin(const std::string& name) const {
    for (const BuiltinInfo& info : kBuiltins) {
        if (name == info.name) return NamedTrace{info.name, info.description, generate(info.name)};
    }
    return std::nullopt;
}

std::vector<std::string> TraceGenerator::builtinNames() {
    std::vector<std::string> names;
    for (const BuiltinInfo& info : kBuiltins) names.push_back(info.name);
    return names;
}
