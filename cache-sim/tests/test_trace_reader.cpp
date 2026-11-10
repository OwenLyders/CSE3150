#include "TestFramework.h"

#include "core/TraceReader.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<MemRequest> parseText(const std::string& text, unsigned addressBits = 32) {
    std::istringstream in(text);
    return TraceReader::parse(in, "test.trace", addressBits);
}

// The line number a malformed trace reports, or 0 if it parses.
std::size_t errorLine(const std::string& text, unsigned addressBits = 32) {
    try {
        parseText(text, addressBits);
    } catch (const TraceError& error) {
        return error.line();
    }
    return 0;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

TEST(parses_the_design_plan_example) {
    const std::vector<MemRequest> trace = parseText(
        "# R <address>   or   W <address> <byte>\n"
        "R 0x1A2E\n"
        "W 0x1A30 0x7F\n"
        "R 0x1A2E\n");
    const std::vector<MemRequest> expected{MemRequest::read(0x1A2E), MemRequest::write(0x1A30, 0x7F),
                                           MemRequest::read(0x1A2E)};
    CHECK_EQ(trace, expected);
}

TEST(skips_blank_lines_and_comments) {
    const std::vector<MemRequest> trace = parseText(
        "\n"
        "   \n"
        "# a comment\n"
        "   # an indented comment\n"
        "R 0x10   # a comment after a request\n"
        "\t\n"
        "W 0x20 5\n");
    const std::vector<MemRequest> expected{MemRequest::read(0x10), MemRequest::write(0x20, 5)};
    CHECK_EQ(trace, expected);
}

TEST(accepts_lowercase_and_decimal) {
    const std::vector<MemRequest> expected{MemRequest::read(16), MemRequest::write(0x1A, 255)};
    CHECK_EQ(parseText("r 16\nw 0x1a 255\n"), expected);
}

TEST(accepts_windows_line_endings_and_no_final_newline) {
    const std::vector<MemRequest> expected{MemRequest::read(0x10), MemRequest::write(0x11, 1)};
    CHECK_EQ(parseText("R 0x10\r\nW 0x11 0x01"), expected);
}

TEST(empty_input_gives_an_empty_trace) {
    CHECK(parseText("").empty());
    CHECK(parseText("# only comments\n\n").empty());
}

TEST(bad_line_reports_its_line_number) {
    const std::string text = "R 0x10\n# comment\n\nX 0x20\nR 0x30\n";
    CHECK_EQ(errorLine(text), 4);
    CHECK(contains(testing::thrownMessage([&] { parseText(text); }), "test.trace:4:"));
}

TEST(rejects_missing_and_extra_fields) {
    CHECK_EQ(errorLine("R\n"), 1);
    CHECK_EQ(errorLine("W 0x10\n"), 1);
    CHECK_EQ(errorLine("R 0x10 0x20\n"), 1);
    CHECK_EQ(errorLine("R 0x10\nW 0x10 0x20 0x30\n"), 2);
}

TEST(rejects_bad_numbers) {
    CHECK_EQ(errorLine("R 0xZZ\n"), 1);
    CHECK_EQ(errorLine("R -5\n"), 1);
    CHECK_EQ(errorLine("R 12abc\n"), 1);
    CHECK_EQ(errorLine("R 0x\n"), 1);
    CHECK_EQ(errorLine("W 0x10 256\n"), 1);      // a byte is at most 0xFF
    CHECK_EQ(errorLine("R 0x100000000\n"), 1);   // more than 32 bits
}

TEST(rejects_addresses_wider_than_the_address_width) {
    CHECK_EQ(errorLine("R 0xFFFF\nR 0x10000\n", 16), 2);
    CHECK_EQ(parseText("R 0xFFFF\n", 16).size(), 1);
}

TEST(loads_every_sample_trace) {
    const std::string dir = CACHESIM_TRACE_DIR;
    CHECK_EQ(TraceReader::load(dir + "/example.trace", 16).size(), 3);
    CHECK_EQ(TraceReader::load(dir + "/lru-wins.trace", 16).size(), 7);
    CHECK_EQ(TraceReader::load(dir + "/fifo-wins.trace", 16).size(), 9);
    CHECK_EQ(TraceReader::load(dir + "/write-back.trace", 16).size(), 7);
}

TEST(missing_file_is_an_error) {
    CHECK_THROWS(TraceReader::load("no/such/file.trace", 16), std::runtime_error);
}

TEST(a_folder_is_not_a_trace_file) {
    CHECK_THROWS(TraceReader::load(CACHESIM_TRACE_DIR, 16), std::runtime_error);
}

// Some Windows editors start UTF-8 files with an invisible byte order mark.
TEST(skips_a_utf8_byte_order_mark) {
    CHECK_EQ(parseText("\xEF\xBB\xBFR 0x10\nR 0x20\n").size(), 2);
}

// Windows PowerShell's > writes UTF-16, which looks like garbage to the parser.
TEST(rejects_utf16_with_a_clear_message) {
    const std::string utf16("\xFF\xFER\0 \0001\0\n\0", 10);
    CHECK_EQ(errorLine(utf16), 1);
    CHECK(contains(testing::thrownMessage([&] { parseText(utf16); }), "UTF-8"));
}
