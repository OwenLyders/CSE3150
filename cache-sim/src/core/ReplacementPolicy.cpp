#include "core/ReplacementPolicy.h"

std::size_t ReplacementPolicy::lineWithSmallestStamp(const std::vector<CacheLine>& set) {
    std::size_t oldest = 0;
    for (std::size_t way = 1; way < set.size(); ++way) {
        if (set[way].stamp < set[oldest].stamp) oldest = way;
    }
    return oldest;
}
