#include "core/TraceReader.h"

#include "core/Format.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <istream>
#include <optional>
#include <sstream>

namespace {

// The whitespace-separated words of a line, ignoring everything from '#' on.
std::vector<std::string> wordsOf(const std::string& line) {
    std::istringstream stream(line.substr(0, line.find('#')));
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) words.push_back(word);
    return words;
}

}  // namespace

TraceError::TraceError(const std::string& source, std::size_t line, const std::string& problem)
    : std::runtime_error(source + ":" + std::to_string(line) + ": " + problem), line_(line) {}

std::vector<MemRequest> TraceReader::load(const std::string& path, unsigned addressBits) {
    std::error_code ignored;
    if (std::filesystem::is_directory(path, ignored)) {
        throw std::runtime_error("'" + path + "' is a folder, not a trace file");
    }
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open trace file '" + path + "'");
    return parse(file, path, addressBits);
}

std::vector<MemRequest> TraceReader::parse(std::istream& in, const std::string& sourceName,
                                           unsigned addressBits) {
    const unsigned widthLimit = std::min(addressBits, 32u);
    std::vector<MemRequest> requests;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(in, line)) {
        ++lineNumber;
        auto error = [&](const std::string& problem) { return TraceError(sourceName, lineNumber, problem); };

        if (lineNumber == 1) {
            if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);  // UTF-8 byte order mark
            if (line.rfind("\xFF\xFE", 0) == 0 || line.rfind("\xFE\xFF", 0) == 0) {
                throw error("this file is UTF-16; save it as UTF-8 or plain ASCII");
            }
        }

        const std::vector<std::string> words = wordsOf(line);
        if (words.empty()) continue;  // a blank line or a comment

        const std::string& op = words[0];
        const bool isRead = (op == "R" || op == "r");
        const bool isWrite = (op == "W" || op == "w");
        if (!isRead && !isWrite) throw error("expected R or W, got '" + op + "'");
        if (isRead && words.size() != 2) throw error("expected 'R <address>'");
        if (isWrite && words.size() != 3) throw error("expected 'W <address> <byte>'");

        const std::optional<std::uint64_t> address = parseNumber(words[1]);
        if (!address) throw error("'" + words[1] + "' is not an address (write it like 0x1A2E or 6702)");
        if ((*address >> widthLimit) != 0) {
            throw error("address " + words[1] + " does not fit in " + std::to_string(widthLimit) + " bits");
        }

        if (isRead) {
            requests.push_back(MemRequest::read(static_cast<Address>(*address)));
            continue;
        }

        const std::optional<std::uint64_t> value = parseNumber(words[2]);
        if (!value) throw error("'" + words[2] + "' is not a byte value (write it like 0x7F or 127)");
        if (*value > 0xFF) throw error("byte value " + words[2] + " is larger than 0xFF");
        requests.push_back(MemRequest::write(static_cast<Address>(*address), static_cast<std::uint8_t>(*value)));
    }
    if (in.bad()) {
        throw std::runtime_error(sourceName + ": read error after line " + std::to_string(lineNumber));
    }
    return requests;
}
