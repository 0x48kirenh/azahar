// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"

namespace Core {

class ARM_Interface;
class System;

class CpuThreadPool {
public:
    explicit CpuThreadPool(System& system);
    ~CpuThreadPool();

    CpuThreadPool(const CpuThreadPool&) = delete;
    CpuThreadPool& operator=(const CpuThreadPool&) = delete;

    void Start();
    void Stop();

    void KickAll();
    void WaitAll();

    bool IsRunning() const {
        return running.load(std::memory_order_acquire);
    }

    u32 NumCores() const {
        return num_cores;
    }

private:
    void ThreadLoop(std::stop_token stop_token, u32 core_id);
    void RunSlice(u32 core_id);

    System& system;
    u32 num_cores = 0;
    std::vector<std::jthread> threads;
    std::atomic_bool running{false};

    std::unique_ptr<Common::SpinBarrier> slice_start_barrier;
    std::unique_ptr<Common::SpinBarrier> slice_end_barrier;
};

} // namespace Core
