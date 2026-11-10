#include "core/Format.h"

#include <algorithm>
#include <cstdio>
#include <limits>

std::string hexDigits(std::uint64_t value, int minDigits) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%0*llX", std::clamp(minDigits, 1, 16),
                  static_cast<unsigned long long>(value));
    return buffer;
}

std::string hex(std::uint64_t value, int minDigits) {
    return "0x" + hexDigits(value, minDigits);
}

int hexWidth(unsigned bits) {
    return bits == 0 ? 1 : static_cast<int>((bits + 3) / 4);
}

std::string binary(std::uint64_t value, unsigned bits) {
    std::string text(bits, '0');
    for (unsigned bit = 0; bit < bits; ++bit) {
        if ((value >> bit) & 1u) text[bits - 1 - bit] = '1';
    }
    return text;
}

std::string percent(double ratio) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f%%", ratio * 100.0);
    return buffer;
}

namespace {

// 0-15 for a hex digit character, or -1.
int digitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::optional<std::uint64_t> parseNumber(const std::string& text) {
    std::uint64_t base = 10;
    std::size_t start = 0;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        start = 2;
    }
    if (start >= text.size()) return std::nullopt;  // empty

    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t value = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        const int digit = digitValue(text[i]);
        if (digit < 0 || static_cast<std::uint64_t>(digit) >= base) return std::nullopt;
        if (value > (max - static_cast<std::uint64_t>(digit)) / base) return std::nullopt;  // overflow
        value = value * base + static_cast<std::uint64_t>(digit);
    }
    return value;
}
