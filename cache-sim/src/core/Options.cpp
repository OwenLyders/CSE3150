#include "core/Options.h"

#include "core/Format.h"
#include "core/TraceGenerator.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {

std::uint64_t wholeNumber(const std::string& flag, const std::string& text, std::uint64_t max) {
    const std::optional<std::uint64_t> value = parseNumber(text);
    if (!value) throw std::invalid_argument(flag + " needs a whole number, got '" + text + "'");
    if (*value > max) throw std::invalid_argument(flag + " value " + text + " is too large");
    return *value;
}

std::string joined(const std::vector<std::string>& words) {
    std::string text;
    for (const std::string& word : words) text += (text.empty() ? "" : ", ") + word;
    return text;
}

}  // namespace

Options parseOptions(const std::vector<std::string>& args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        // Accept both "--cache 64" and "--cache=64".
        std::string flag = args[i];
        std::optional<std::string> attachedValue;
        const std::size_t equals = flag.find('=');
        if (flag.rfind("--", 0) == 0 && equals != std::string::npos) {
            attachedValue = flag.substr(equals + 1);
            flag = flag.substr(0, equals);
        }

        if (flag == "-h" || flag == "--help") {
            options.showHelp = true;
            return options;
        }

        auto value = [&]() -> std::string {
            if (attachedValue) return *attachedValue;
            if (i + 1 >= args.size()) throw std::invalid_argument(flag + " needs a value");
            return args[++i];
        };

        // Reject anything size_t can't hold (on a 32-bit build, 2^32 + 64 must not become 64).
        const std::uint64_t maxSize = std::numeric_limits<std::size_t>::max();
        if (flag == "--cache") {
            options.config.cacheSize = static_cast<std::size_t>(wholeNumber(flag, value(), maxSize));
        } else if (flag == "--block") {
            options.config.blockSize = static_cast<std::size_t>(wholeNumber(flag, value(), maxSize));
        } else if (flag == "--ways") {
            options.config.associativity = static_cast<std::size_t>(wholeNumber(flag, value(), maxSize));
        } else if (flag == "--address-bits") {
            options.config.addressBits = static_cast<unsigned>(wholeNumber(flag, value(), 64));
        } else if (flag == "--trace") {
            options.traceFile = value();
        } else if (flag == "--builtin") {
            options.builtin = value();
        } else {
            throw std::invalid_argument("unknown option '" + args[i] + "'");
        }
    }

    options.config.validate();

    if (options.traceFile && options.builtin) {
        throw std::invalid_argument("use either --trace or --builtin, not both");
    }
    if (options.builtin) {
        const std::vector<std::string> names = TraceGenerator::builtinNames();
        if (std::find(names.begin(), names.end(), *options.builtin) == names.end()) {
            throw std::invalid_argument("unknown built-in trace '" + *options.builtin + "' (choose from " +
                                        joined(names) + ")");
        }
    }
    return options;
}

std::string optionsHelp() {
    return "Cache settings (block, cache and ways must be powers of two):\n"
           "  --cache BYTES         total cache size                (default 64)\n"
           "  --block BYTES         block size                      (default 4)\n"
           "  --ways N              associativity: lines per set    (default 4)\n"
           "  --address-bits N      address width, 1 to " +
           std::to_string(CacheConfig::kMaxAddressBits) +
           "             (default 16)\n"
           "\n"
           "Trace:\n"
           "  --trace FILE          read requests from a trace file\n"
           "  --builtin NAME        a built-in trace: " +
           joined(TraceGenerator::builtinNames()) +
           "\n"
           "\n"
           "  -h, --help            show this help\n";
}
