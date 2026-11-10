#pragma once

#include "core/ReplacementPolicy.h"

#include <memory>

// The policies the simulator can run. To add one (Random, LFU, ...): write a class like
// FifoPolicy in its own header, add a value here, and add a case to makePolicy().
enum class PolicyKind { Fifo, Lru };

std::unique_ptr<ReplacementPolicy> makePolicy(PolicyKind kind);
