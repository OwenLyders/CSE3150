#include "core/PolicyFactory.h"

#include "core/FifoPolicy.h"
#include "core/LruPolicy.h"

#include <stdexcept>

std::unique_ptr<ReplacementPolicy> makePolicy(PolicyKind kind) {
    switch (kind) {
        case PolicyKind::Fifo:
            return std::make_unique<FifoPolicy>();
        case PolicyKind::Lru:
            return std::make_unique<LruPolicy>();
    }
    throw std::invalid_argument("unknown policy kind");
}
