// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <mutex>

#include "common/common_types.h"

namespace Core {

class InterruptController {
public:
    static constexpr u32 MAX_CORES = 4;
    static constexpr u32 MAX_INTERRUPTS = 64;

    enum class IrqType : u32 {
        PSC0 = 0x00,
        PSC1 = 0x01,
        DMA = 0x04,
        TIMER_0 = 0x10,
        TIMER_1 = 0x11,
        TIMER_2 = 0x12,
        TIMER_3 = 0x13,
        VBLANK = 0x1F,
        NDM = 0x20,
        MCU = 0x2E,
    };

    InterruptController() = default;

    void SignalInterrupt(IrqType irq, u32 target_core = 0);

    void ClearInterrupt(IrqType irq, u32 core_id);

    bool CheckPending(u32 core_id) const {
        return pending_irq[core_id].load(std::memory_order_acquire) != 0;
    }

    u32 AcknowledgePending(u32 core_id) {
        u32 bits = pending_irq[core_id].load(std::memory_order_acquire);
        u32 ack = bits & ~masked_irq[core_id].load(std::memory_order_acquire);
        return ack;
    }

    u32 ReadPendingReg(u32 core_id) const {
        return pending_irq[core_id].load(std::memory_order_acquire);
    }

    void SetInterruptMask(u32 core_id, u32 mask) {
        masked_irq[core_id].store(mask, std::memory_order_release);
    }

private:
    std::atomic<u32> pending_irq[MAX_CORES]{};
    std::atomic<u32> masked_irq[MAX_CORES]{};
    std::mutex signal_mutex;
};

} // namespace Core
