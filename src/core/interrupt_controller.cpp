// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/interrupt_controller.h"
#include "common/logging/log.h"

namespace Core {

void InterruptController::SignalInterrupt(IrqType irq, u32 target_core) {
    if (target_core >= MAX_CORES) {
        LOG_ERROR(Core, "InterruptController: invalid target core {}", target_core);
        return;
    }
    const u32 bit = 1u << static_cast<u32>(irq);
    pending_irq[target_core].fetch_or(bit, std::memory_order_acq_rel);
}

void InterruptController::ClearInterrupt(IrqType irq, u32 core_id) {
    if (core_id >= MAX_CORES) {
        return;
    }
    const u32 bit = 1u << static_cast<u32>(irq);
    pending_irq[core_id].fetch_and(~bit, std::memory_order_acq_rel);
}

} // namespace Core
