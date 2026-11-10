#include "core/MainMemory.h"

#include "core/Format.h"

#include <algorithm>
#include <stdexcept>
#include <string>

MainMemory::MainMemory(std::size_t sizeBytes) : bytes_(sizeBytes) {
    for (std::size_t address = 0; address < bytes_.size(); ++address) {
        bytes_[address] = static_cast<std::uint8_t>(address & 0xFF);
    }
}

std::size_t MainMemory::size() const {
    return bytes_.size();
}

std::uint8_t MainMemory::readByte(Address address) const {
    checkRange(address, 1);
    return bytes_[address];
}

std::vector<std::uint8_t> MainMemory::readBlock(Address start, std::size_t length) const {
    checkRange(start, length);
    const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(start);
    return std::vector<std::uint8_t>(first, first + static_cast<std::ptrdiff_t>(length));
}

void MainMemory::writeBlock(Address start, const std::vector<std::uint8_t>& bytes) {
    checkRange(start, bytes.size());
    std::copy(bytes.begin(), bytes.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(start));
}

void MainMemory::checkRange(Address start, std::size_t length) const {
    if (start > bytes_.size() || length > bytes_.size() - start) {
        throw std::out_of_range("memory access at " + hex(start) + " (" + std::to_string(length) +
                                " bytes) is outside the " + std::to_string(bytes_.size()) + "-byte memory");
    }
}
