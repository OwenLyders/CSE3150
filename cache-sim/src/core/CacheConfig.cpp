#include "core/CacheConfig.h"

#include <stdexcept>
#include <string>

namespace {

bool isPowerOfTwo(std::size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

// log2 of a power of two: log2Exact(16) == 4.
unsigned log2Exact(std::size_t n) {
    unsigned bits = 0;
    while (n > 1) {
        n >>= 1;
        ++bits;
    }
    return bits;
}

std::string bytes(std::size_t n) {
    return std::to_string(n) + " B";
}

}  // namespace

void CacheConfig::validate() const {
    if (addressBits < 1 || addressBits > kMaxAddressBits) {
        throw std::invalid_argument("address width must be 1 to " + std::to_string(kMaxAddressBits) +
                                    " bits (got " + std::to_string(addressBits) + ")");
    }
    if (!isPowerOfTwo(blockSize)) {
        throw std::invalid_argument("block size must be a power of two (got " + std::to_string(blockSize) + ")");
    }
    if (!isPowerOfTwo(cacheSize)) {
        throw std::invalid_argument("cache size must be a power of two (got " + std::to_string(cacheSize) + ")");
    }
    if (cacheSize > kMaxCacheSize) {
        throw std::invalid_argument("cache size can be at most " + bytes(kMaxCacheSize) + " (got " +
                                    bytes(cacheSize) + ")");
    }
    if (!isPowerOfTwo(associativity)) {
        throw std::invalid_argument("associativity must be a power of two (got " +
                                    std::to_string(associativity) + ")");
    }
    if (blockSize > cacheSize) {
        throw std::invalid_argument("block size (" + bytes(blockSize) + ") is larger than the cache (" +
                                    bytes(cacheSize) + ")");
    }
    if (associativity > numBlocks()) {
        throw std::invalid_argument("associativity (" + std::to_string(associativity) +
                                    " ways) is more than the " + std::to_string(numBlocks()) +
                                    " blocks the cache holds");
    }
    if (2 * cacheSize > memorySize()) {
        throw std::invalid_argument("main memory (" + bytes(memorySize()) + " with " + std::to_string(addressBits) +
                                    "-bit addresses) must be at least twice the cache (" + bytes(cacheSize) +
                                    ")");
    }
}

std::size_t CacheConfig::memorySize() const {
    return std::size_t{1} << addressBits;
}

std::size_t CacheConfig::numBlocks() const {
    return cacheSize / blockSize;
}

std::size_t CacheConfig::numSets() const {
    return numBlocks() / associativity;
}

unsigned CacheConfig::offsetBits() const {
    return log2Exact(blockSize);
}

unsigned CacheConfig::indexBits() const {
    return log2Exact(numSets());
}

unsigned CacheConfig::tagBits() const {
    return addressBits - indexBits() - offsetBits();
}

AddressParts CacheConfig::split(Address address) const {
    AddressParts parts;
    parts.offset = address & (blockSize - 1);
    parts.index = (address >> offsetBits()) & (numSets() - 1);
    parts.tag = address >> (offsetBits() + indexBits());
    return parts;
}

Address CacheConfig::blockAddress(std::uint32_t tag, std::size_t setIndex) const {
    const Address tagPart = static_cast<Address>(tag) << (indexBits() + offsetBits());
    const Address indexPart = static_cast<Address>(setIndex) << offsetBits();
    return tagPart | indexPart;
}

std::string CacheConfig::describe() const {
    std::string organization;
    if (associativity == 1) {
        organization = "direct-mapped";
    } else if (numSets() == 1) {
        organization = "fully associative";
    } else {
        organization = std::to_string(associativity) + "-way set associative";
    }
    return bytes(cacheSize) + " cache, " + bytes(blockSize) + " blocks, " + organization + " (" +
           std::to_string(numSets()) + " sets x " + std::to_string(associativity) + " ways)";
}
