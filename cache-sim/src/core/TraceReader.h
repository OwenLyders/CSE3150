#pragma once

#include "core/MemRequest.h"

#include <cstddef>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

// A malformed trace line. what() reads "<file>:<line>: <problem>".
class TraceError : public std::runtime_error {
public:
    TraceError(const std::string& source, std::size_t line, const std::string& problem);
    std::size_t line() const { return line_; }

private:
    std::size_t line_;
};

// Reads trace files: one request per line, "R <address>" or "W <address> <byte>".
// Numbers are hex with a 0x prefix, or plain decimal. Blank lines and lines starting
// with # are skipped, and a # after a request starts a comment. The file must be ASCII
// or UTF-8 (a UTF-8 byte order mark is fine).
class TraceReader {
public:
    // Throws std::runtime_error if the file cannot be opened or read, and TraceError (with
    // the line number) for the first malformed line, including an address wider than
    // `addressBits` bits. Pass the cache's address width, so a bad address is reported
    // with its line number.
    static std::vector<MemRequest> load(const std::string& path, unsigned addressBits);

    static std::vector<MemRequest> parse(std::istream& in, const std::string& sourceName, unsigned addressBits);
};
