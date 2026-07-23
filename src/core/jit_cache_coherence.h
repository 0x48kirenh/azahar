// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstddef>

#include "common/common_types.h"

namespace Core {

class JitCacheCoherence {
public:
    static constexpr u32 MAX_CORES = 4;

    JitCacheCoherence() = default;

    void InvalidateRegion(VAddr start, std::size_t length) {
        global_generation.fetch_add(1, std::memory_order_acq_rel);
    }

    void InvalidateAll() {
        global_generation.fetch_add(1, std::memory_order_acq_rel);
    }

    u64 CurrentGeneration() const {
        return global_generation.load(std::memory_order_acquire);
    }

    bool IsStale(u32 core_id, u64 core_generation) const {
        if (core_id >= MAX_CORES)
            return false;
        return core_generation != global_generation.load(std::memory_order_acquire);
    }

    void MarkCoreClean(u32 core_id, u64& core_generation) {
        if (core_id < MAX_CORES) {
            core_generation = global_generation.load(std::memory_order_acquire);
        }
    }

private:
    std::atomic<u64> global_generation{1};
};

} // namespace Core
