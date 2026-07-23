// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>

#include "common/common_types.h"
#include "common/threadsafe_queue.h"

namespace Core {

struct CrossCoreEvent {
    enum class Type : u32 {
        SendSyncRequest,
        ArbitrateAddress,
        ProcessYield,
        KernelCallback,
        InterruptSignal,
        None,
    };

    Type type = Type::None;
    u32 sender_core = 0;
    u32 param0 = 0;
    u32 param1 = 0;
    u64 param2 = 0;
};

class CrossCoreSVC {
public:
    static constexpr u32 MAX_CORES = 4;

    CrossCoreSVC() = default;

    void Send(u32 target_core, CrossCoreEvent event) {
        if (target_core < MAX_CORES) {
            queues[target_core].Push(std::move(event));
        }
    }

    bool Poll(u32 core_id, CrossCoreEvent& out) {
        if (core_id >= MAX_CORES)
            return false;
        return queues[core_id].Pop(out);
    }

    bool HasPending(u32 core_id) const {
        if (core_id >= MAX_CORES)
            return false;
        return !queues[core_id].Empty();
    }

    void Clear(u32 core_id) {
        if (core_id < MAX_CORES) {
            queues[core_id].Clear();
        }
    }

    void ClearAll() {
        for (auto& q : queues) {
            q.Clear();
        }
    }

private:
    std::array<Common::MPSCQueue<CrossCoreEvent>, MAX_CORES> queues;
};

} // namespace Core
