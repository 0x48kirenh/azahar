// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/microprofile.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_thread_pool.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/memory.h"

namespace Core {

CpuThreadPool::CpuThreadPool(System& system)
    : system(system) {}

CpuThreadPool::~CpuThreadPool() {
    Stop();
}

void CpuThreadPool::Start() {
    if (running.load(std::memory_order_acquire)) {
        return;
    }

    num_cores = system.GetNumCores();
    if (num_cores == 0) {
        return;
    }

    slice_start_barrier = std::make_unique<Common::SpinBarrier>(num_cores + 1);
    slice_end_barrier = std::make_unique<Common::SpinBarrier>(num_cores + 1);

    running.store(true, std::memory_order_release);
    threads.reserve(num_cores);
    for (u32 i = 0; i < num_cores; ++i) {
        threads.emplace_back([this, i](std::stop_token token) { ThreadLoop(token, i); });
    }
}

void CpuThreadPool::Stop() {
    running.store(false, std::memory_order_release);
    for (auto& t : threads) {
        t.request_stop();
    }
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads.clear();
}

void CpuThreadPool::KickAll() {
    slice_start_barrier->Sync();
}

void CpuThreadPool::WaitAll() {
    slice_end_barrier->Sync();
}

void CpuThreadPool::ThreadLoop(std::stop_token stop_token, u32 core_id) {
    Common::SetCurrentThreadName(core_id == 0 ? "CPU_Core0" :
                                 core_id == 1 ? "CPU_Core1" :
                                 core_id == 2 ? "CPU_Core2" : "CPU_Core3");

    while (running.load(std::memory_order_acquire) && !stop_token.stop_requested()) {
        if (!slice_start_barrier->Sync(stop_token)) {
            break;
        }

        if (!running.load(std::memory_order_acquire) || stop_token.stop_requested()) {
            break;
        }

        RunSlice(core_id);

        if (!slice_end_barrier->Sync(stop_token)) {
            break;
        }
    }
}

void CpuThreadPool::RunSlice(u32 core_id) {
    MICROPROFILE_SCOPE(CPU_AdvancePhase);

    auto& cpu = system.GetCore(core_id);
    auto& kernel = system.Kernel();
    auto& thread_manager = kernel.GetThreadManager(core_id);

    {
        std::scoped_lock lock{kernel.GetHLELock()};
        kernel.SetRunningCPU(&cpu);
        system.CoreTiming().SetCurrentTimer(core_id);

        cpu.GetTimer().Advance();
        cpu.PrepareReschedule();
        thread_manager.Reschedule();
    }

    // SetNextSlice doesn't need the HLE lock — it only touches this core's own timer.
    cpu.GetTimer().SetNextSlice(Timing::MAX_SLICE_LENGTH);

    MICROPROFILE_SCOPE(CPU_ExecutePhase);

    auto* current_thread = thread_manager.GetCurrentThread();
    if (current_thread != nullptr) {
        cpu.Run();
        // If the thread yielded/blocked without consuming the full slice,
        // idle the remaining ticks so the timer advances properly.
        if (cpu.GetTimer().GetDowncount() > 0) {
            cpu.GetTimer().Idle();
        }
    } else {
        cpu.GetTimer().Idle();
        system.PrepareReschedule();
    }
}

} // namespace Core
