// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "common/threadsafe_queue.h"

namespace Core {
class System;
}

namespace Service::GSP {
struct Command;
struct FrameBufferInfo;
}

namespace VideoCore {

enum class GpuCommandType : u32 {
    Execute,
    MemoryFill,
    MemoryTransfer,
    SubmitCmdList,
    FlushRegion,
    InvalidateRegion,
    ClearAll,
    Shutdown,
};

struct GpuCommand {
    GpuCommandType type = GpuCommandType::Execute;
    u32 param0 = 0;
    u32 param1 = 0;
    VAddr param_addr = 0;
    PAddr param_paddr = 0;
    u64 param_u64 = 0;
    bool param_bool = false;
    const Service::GSP::Command* gsp_command = nullptr;
    const Service::GSP::FrameBufferInfo* fb_info = nullptr;
    std::atomic_int* completion_flag = nullptr;
};

class GpuCommandThread {
public:
    explicit GpuCommandThread(Core::System& system);
    ~GpuCommandThread();

    GpuCommandThread(const GpuCommandThread&) = delete;
    GpuCommandThread& operator=(const GpuCommandThread&) = delete;

    void Start();
    void Stop();

    void SubmitCommand(GpuCommand cmd);

    bool IsRunning() const {
        return running.load(std::memory_order_acquire);
    }

private:
    void ThreadLoop(std::stop_token stop_token);

    Core::System& system;
    Common::MPSCQueue<GpuCommand, true> command_queue;
    std::jthread worker_thread;
    std::atomic_bool running{false};
};

} // namespace VideoCore
