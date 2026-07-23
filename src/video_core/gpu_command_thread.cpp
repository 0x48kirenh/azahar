// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "video_core/gpu.h"
#include "video_core/gpu_command_thread.h"
#include "video_core/pica/pica_core.h"
#include "video_core/renderer_base.h"

namespace VideoCore {

GpuCommandThread::GpuCommandThread(Core::System& system) : system(system) {}

GpuCommandThread::~GpuCommandThread() {
    Stop();
}

void GpuCommandThread::Start() {
    if (running.load(std::memory_order_acquire)) {
        return;
    }
    running.store(true, std::memory_order_release);
    worker_thread = std::jthread([this](std::stop_token token) { ThreadLoop(token); });
}

void GpuCommandThread::Stop() {
    if (!running.load(std::memory_order_acquire)) {
        return;
    }
    running.store(false, std::memory_order_release);
    GpuCommand shutdown_cmd;
    shutdown_cmd.type = GpuCommandType::Shutdown;
    command_queue.Push(shutdown_cmd);
    if (worker_thread.joinable()) {
        worker_thread.request_stop();
        worker_thread.join();
    }
}

void GpuCommandThread::SubmitCommand(GpuCommand cmd) {
    command_queue.Push(cmd);
}

void GpuCommandThread::ThreadLoop(std::stop_token stop_token) {
    Common::SetCurrentThreadName("GPU_CommandThread");

    auto& gpu = system.GPU();
    auto& memory = system.Memory();
    auto& renderer = gpu.Renderer();
    auto& pica = gpu.PicaCore();

    while (running.load(std::memory_order_acquire) && !stop_token.stop_requested()) {
        GpuCommand cmd = command_queue.PopWait(stop_token);
        if (stop_token.stop_requested()) {
            break;
        }

        switch (cmd.type) {
        case GpuCommandType::Shutdown:
            break;

        case GpuCommandType::Execute:
            if (cmd.gsp_command) {
                gpu.ExecuteSync(*cmd.gsp_command);
            }
            break;

        case GpuCommandType::MemoryFill:
            gpu.MemoryFillSync(cmd.param0, cmd.param1);
            break;

        case GpuCommandType::MemoryTransfer:
            gpu.MemoryTransferSync();
            break;

        case GpuCommandType::SubmitCmdList:
            gpu.SubmitCmdListSync(cmd.param0);
            break;

        case GpuCommandType::FlushRegion:
            renderer.Rasterizer()->FlushRegion(cmd.param_paddr, cmd.param0);
            break;

        case GpuCommandType::InvalidateRegion:
            renderer.Rasterizer()->InvalidateRegion(cmd.param_paddr, cmd.param0);
            break;

        case GpuCommandType::ClearAll:
            renderer.Rasterizer()->ClearAll(cmd.param_bool);
            break;

        default:
            break;
        }

        if (cmd.completion_flag) {
            cmd.completion_flag->store(1, std::memory_order_release);
            cmd.completion_flag->notify_one();
        }
    }
}

} // namespace VideoCore
