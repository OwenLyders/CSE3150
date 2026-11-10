#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Text helpers shared by the trace reader, the command-line runner and the GUI.

// Upper-case hex digits, zero-padded to at least minDigits: hexDigits(0x2E, 4) == "002E".
std::string hexDigits(std::uint64_t value, int minDigits = 1);

// hexDigits with a 0x prefix: hex(0x1A2E, 4) == "0x1A2E".
std::string hex(std::uint64_t value, int minDigits = 1);

// How many hex digits a field `bits` wide needs (at least 1): hexWidth(12) == 3.
int hexWidth(unsigned bits);

// The low `bits` bits of value, most significant first: binary(5, 4) == "0101".
std::string binary(std::uint64_t value, unsigned bits);

// A ratio as a percentage with one decimal place: percent(1.0 / 3) == "33.3%".
std::string percent(double ratio);

// Parses "0x1A2E" (hex, either case) or "6702" (decimal). Returns nothing for anything
// else, including signs, spaces, an empty string, or a value too big for 64 bits.
std::optional<std::uint64_t> parseNumber(const std::string& text);
