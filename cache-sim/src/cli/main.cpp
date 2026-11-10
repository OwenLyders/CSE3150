// cachesim_cli: replays traces through a FIFO cache and an LRU cache and prints a results
// table. It uses the same Simulator class as the GUI, so the two always agree.
//
//   cachesim_cli                                   every built-in trace, default cache
//   cachesim_cli --builtin hot-cold --ways 2
//   cachesim_cli --trace traces/lru-wins.trace

#include "core/Format.h"
#include "core/Options.h"
#include "core/Simulator.h"
#include "core/TraceGenerator.h"
#include "core/TraceReader.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

void printUsage() {
    std::cout << "usage: cachesim_cli [options]\n"
                 "\n"
                 "Replays memory traces through two identical caches, one evicting with FIFO and one\n"
                 "with LRU, and prints accesses, hits, misses, evictions, write-backs and hit rate.\n"
                 "With neither --trace nor --builtin it runs every built-in trace.\n"
                 "\n"
              << optionsHelp();
}

void printConfig(const CacheConfig& config) {
    std::cout << "Cache:   " << config.describe() << "\n"
              << "Address: " << config.addressBits << " bits = " << config.tagBits() << " tag + "
              << config.indexBits() << " index + " << config.offsetBits() << " offset\n";
}

void printResults(const NamedTrace& trace, const Simulator& sim) {
    std::cout << "\n" << trace.name << ": " << trace.description << " (" << trace.requests.size()
              << " requests)\n";
    std::cout << "  " << std::left << std::setw(8) << "Policy" << std::right << std::setw(10) << "Accesses"
              << std::setw(8) << "Hits" << std::setw(8) << "Misses" << std::setw(11) << "Evictions"
              << std::setw(13) << "Write-backs" << std::setw(10) << "Hit rate" << "\n";
    for (std::size_t c = 0; c < sim.cacheCount(); ++c) {
        const Cache& cache = sim.cache(c);
        const CacheStats& stats = cache.stats();
        std::cout << "  " << std::left << std::setw(8) << cache.policyName() << std::right << std::setw(10)
                  << stats.accesses << std::setw(8) << stats.hits << std::setw(8) << stats.misses
                  << std::setw(11) << stats.evictions << std::setw(13) << stats.writeBacks << std::setw(10)
                  << percent(stats.hitRate()) << "\n";
    }
}

struct SummaryRow {
    std::string trace;
    std::vector<double> hitRates;  // one per policy
};

void printSummary(const std::vector<std::string>& policies, const std::vector<SummaryRow>& rows) {
    std::cout << "\nHit rate summary\n  " << std::left << std::setw(18) << "Trace" << std::right;
    for (const std::string& policy : policies) std::cout << std::setw(9) << policy;
    std::cout << "\n";
    for (const SummaryRow& row : rows) {
        std::cout << "  " << std::left << std::setw(18) << row.trace << std::right;
        for (double rate : row.hitRates) std::cout << std::setw(9) << percent(rate);
        std::cout << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    Options options;
    try {
        options = parseOptions(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const std::exception& error) {
        std::cerr << "cachesim_cli: " << error.what() << "\n(run with --help for usage)\n";
        return 2;
    }
    if (options.showHelp) {
        printUsage();
        return 0;
    }

    std::vector<NamedTrace> traces;
    try {
        if (options.traceFile) {
            traces.push_back({*options.traceFile, "trace file",
                              TraceReader::load(*options.traceFile, options.config.addressBits)});
        } else if (options.builtin) {
            traces.push_back(*TraceGenerator(options.config).builtin(*options.builtin));
        } else {
            traces = TraceGenerator(options.config).builtins();
        }
    } catch (const std::exception& error) {
        std::cerr << "cachesim_cli: " << error.what() << "\n";
        return 1;
    }

    printConfig(options.config);

    std::vector<std::string> policies;
    std::vector<SummaryRow> summary;
    for (const NamedTrace& trace : traces) {
        Simulator sim(options.config, trace.requests);
        sim.run();
        printResults(trace, sim);

        SummaryRow row{trace.name, {}};
        policies.clear();
        for (std::size_t c = 0; c < sim.cacheCount(); ++c) {
            policies.push_back(sim.cache(c).policyName());
            row.hitRates.push_back(sim.cache(c).stats().hitRate());
        }
        summary.push_back(row);
    }
    if (summary.size() > 1) printSummary(policies, summary);
    return 0;
}
