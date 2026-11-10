#include "core/MemRequest.h"

#include "core/Format.h"

#include <ostream>

MemRequest MemRequest::read(Address address) {
    MemRequest request;
    request.op = Op::Read;
    request.address = address;
    return request;
}

MemRequest MemRequest::write(Address address, std::uint8_t value) {
    MemRequest request;
    request.op = Op::Write;
    request.address = address;
    request.value = value;
    return request;
}

bool operator==(const MemRequest& a, const MemRequest& b) {
    return a.op == b.op && a.address == b.address && a.value == b.value;
}

bool operator!=(const MemRequest& a, const MemRequest& b) {
    return !(a == b);
}

std::string toString(const MemRequest& request) {
    if (request.op == Op::Read) return "R " + hex(request.address, 4);
    return "W " + hex(request.address, 4) + " " + hex(request.value, 2);
}

std::ostream& operator<<(std::ostream& out, const MemRequest& request) {
    return out << toString(request);
}
