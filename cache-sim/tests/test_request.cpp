#include "TestFramework.h"

#include "core/MemRequest.h"

TEST(read_and_write_helpers_fill_every_field) {
    const MemRequest read = MemRequest::read(0x1A2E);
    CHECK(read.op == Op::Read);
    CHECK_EQ(read.address, 0x1A2E);
    CHECK_EQ(read.value, 0);

    const MemRequest write = MemRequest::write(0x1A30, 0x7F);
    CHECK(write.op == Op::Write);
    CHECK_EQ(write.address, 0x1A30);
    CHECK_EQ(write.value, 0x7F);
}

TEST(requests_compare_by_every_field) {
    CHECK(MemRequest::read(0x10) == MemRequest::read(0x10));
    CHECK(MemRequest::read(0x10) != MemRequest::read(0x11));
    CHECK(MemRequest::read(0x10) != MemRequest::write(0x10, 0));
    CHECK(MemRequest::write(0x10, 1) != MemRequest::write(0x10, 2));
}

TEST(to_string_uses_trace_file_syntax) {
    CHECK_EQ(toString(MemRequest::read(0x1A2E)), "R 0x1A2E");
    CHECK_EQ(toString(MemRequest::write(0x1A30, 0x7F)), "W 0x1A30 0x7F");
    CHECK_EQ(toString(MemRequest::read(0x10)), "R 0x0010");
}
