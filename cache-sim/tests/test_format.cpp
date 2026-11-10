#include "TestFramework.h"

#include "core/Format.h"

TEST(hex_pads_to_minimum_digits) {
    CHECK_EQ(hex(0x1A2E, 4), "0x1A2E");
    CHECK_EQ(hex(0x2E, 4), "0x002E");
    CHECK_EQ(hexDigits(0x7F, 2), "7F");
    CHECK_EQ(hexDigits(0, 1), "0");
}

TEST(hex_never_truncates) {
    CHECK_EQ(hexDigits(0xABCDE, 2), "ABCDE");
}

TEST(hex_width_rounds_up_to_whole_digits) {
    CHECK_EQ(hexWidth(12), 3);
    CHECK_EQ(hexWidth(13), 4);
    CHECK_EQ(hexWidth(1), 1);
    CHECK_EQ(hexWidth(0), 1);
}

TEST(binary_shows_exactly_the_requested_bits) {
    CHECK_EQ(binary(0x1A2E, 16), "0001101000101110");
    CHECK_EQ(binary(5, 4), "0101");
    CHECK_EQ(binary(0xFF, 0), "");
}

TEST(percent_has_one_decimal_place) {
    CHECK_EQ(percent(1.0 / 3.0), "33.3%");
    CHECK_EQ(percent(0.0), "0.0%");
    CHECK_EQ(percent(1.0), "100.0%");
}

TEST(parse_number_reads_hex_and_decimal) {
    CHECK_EQ(parseNumber("0x1A2E").value_or(0), 0x1A2E);
    CHECK_EQ(parseNumber("0x1a2e").value_or(0), 0x1A2E);
    CHECK_EQ(parseNumber("0X7F").value_or(0), 0x7F);
    CHECK_EQ(parseNumber("6702").value_or(0), 6702);
    CHECK_EQ(parseNumber("0").value_or(1), 0);
}

TEST(parse_number_rejects_anything_else) {
    CHECK(!parseNumber(""));
    CHECK(!parseNumber("0x"));
    CHECK(!parseNumber("-5"));
    CHECK(!parseNumber("+5"));
    CHECK(!parseNumber("12abc"));
    CHECK(!parseNumber("0xZZ"));
    CHECK(!parseNumber(" 12"));
    CHECK(!parseNumber("99999999999999999999999"));  // more than 64 bits
}
