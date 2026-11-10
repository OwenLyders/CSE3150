#pragma once

#include "core/CacheConfig.h"

#include <optional>
#include <string>
#include <vector>

// Command-line settings shared by the GUI and the command-line runner, for example
// --cache 64 --block 4 --ways 4 --builtin hot-cold
struct Options {
    CacheConfig config;                    // already validated
    std::optional<std::string> traceFile;  // --trace FILE
    std::optional<std::string> builtin;    // --builtin NAME
    bool showHelp = false;                 // -h or --help
};

// `args` excludes the program name. Throws std::invalid_argument with a message ready
// to print. Checks the cache settings and the built-in trace name.
Options parseOptions(const std::vector<std::string>& args);

// The help text for the shared flags.
std::string optionsHelp();
