#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

// A byte address. The address width (16 bits by default) is a CacheConfig setting;
// 32 bits is just the widest type any setting needs.
using Address = std::uint32_t;

enum class Op { Read, Write };

// One entry in a memory-access trace: read one byte, or write one byte.
struct MemRequest {
    Op op = Op::Read;
    Address address = 0;
    std::uint8_t value = 0;  // the byte to store; ignored for reads

    static MemRequest read(Address address);
    static MemRequest write(Address address, std::uint8_t value);
};

bool operator==(const MemRequest& a, const MemRequest& b);
bool operator!=(const MemRequest& a, const MemRequest& b);

// The request in trace-file syntax: "R 0x1A2E" or "W 0x1A30 0x7F".
std::string toString(const MemRequest& request);
std::ostream& operator<<(std::ostream& out, const MemRequest& request);
