// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <shared_mutex>
#include <type_traits>

#include "common/common_types.h"
#include "core/memory.h"

namespace Memory {

class PageLock {
    static constexpr std::size_t PAGE_SIZE = CITRA_PAGE_SIZE;
    static constexpr std::size_t STRIPE_PAGES = 16;
    static constexpr std::size_t STRIPE_SIZE = STRIPE_PAGES * PAGE_SIZE;

    static constexpr std::size_t FCRAM_N3DS_PAGE_COUNT = Memory::FCRAM_N3DS_SIZE / PAGE_SIZE;
    static constexpr std::size_t FCRAM_STRIPE_COUNT = (FCRAM_N3DS_PAGE_COUNT + STRIPE_PAGES - 1) / STRIPE_PAGES;

    static constexpr std::size_t VRAM_PAGE_COUNT = Memory::VRAM_SIZE / PAGE_SIZE;
    static constexpr std::size_t VRAM_STRIPE_COUNT = (VRAM_PAGE_COUNT + STRIPE_PAGES - 1) / STRIPE_PAGES;

    static constexpr std::size_t DSP_PAGE_COUNT = Memory::DSP_RAM_SIZE / PAGE_SIZE;
    static constexpr std::size_t DSP_STRIPE_COUNT = (DSP_PAGE_COUNT + STRIPE_PAGES - 1) / STRIPE_PAGES;

    static constexpr std::size_t N3DS_PAGE_COUNT = Memory::N3DS_EXTRA_RAM_SIZE / PAGE_SIZE;
    static constexpr std::size_t N3DS_STRIPE_COUNT = (N3DS_PAGE_COUNT + STRIPE_PAGES - 1) / STRIPE_PAGES;

    struct Stripe {
        alignas(64) std::shared_mutex mutex;
    };

    static_assert(sizeof(Stripe) <= 64, "Stripe must fit in one cache line");

    std::array<Stripe, FCRAM_STRIPE_COUNT> fcram_stripes{};
    std::array<Stripe, VRAM_STRIPE_COUNT> vram_stripes{};
    std::array<Stripe, DSP_STRIPE_COUNT> dsp_stripes{};
    std::array<Stripe, N3DS_STRIPE_COUNT> n3ds_stripes{};

    static constexpr std::size_t REGION_STRIPE_SHIFT = 12 + 4;

    struct RegionInfo {
        Stripe* stripes;
        PAddr base;
        PAddr end;
        std::size_t count;
    };

    const RegionInfo* FindRegionInfo(PAddr paddr) const {
        if (paddr >= Memory::FCRAM_PADDR && paddr < Memory::FCRAM_N3DS_PADDR_END) {
            static const RegionInfo info{const_cast<Stripe*>(fcram_stripes.data()), Memory::FCRAM_PADDR,
                                         Memory::FCRAM_N3DS_PADDR_END, FCRAM_STRIPE_COUNT};
            return &info;
        }
        if (paddr >= Memory::VRAM_PADDR && paddr < Memory::VRAM_PADDR_END) {
            static const RegionInfo info{const_cast<Stripe*>(vram_stripes.data()), Memory::VRAM_PADDR,
                                         Memory::VRAM_PADDR_END, VRAM_STRIPE_COUNT};
            return &info;
        }
        if (paddr >= Memory::DSP_RAM_PADDR && paddr < Memory::DSP_RAM_PADDR_END) {
            static const RegionInfo info{const_cast<Stripe*>(dsp_stripes.data()), Memory::DSP_RAM_PADDR,
                                         Memory::DSP_RAM_PADDR_END, DSP_STRIPE_COUNT};
            return &info;
        }
        if (paddr >= Memory::N3DS_EXTRA_RAM_PADDR && paddr < Memory::N3DS_EXTRA_RAM_PADDR_END) {
            static const RegionInfo info{const_cast<Stripe*>(n3ds_stripes.data()), Memory::N3DS_EXTRA_RAM_PADDR,
                                         Memory::N3DS_EXTRA_RAM_PADDR_END, N3DS_STRIPE_COUNT};
            return &info;
        }
        return nullptr;
    }

    static std::size_t StripeIndex(PAddr paddr) {
        return (paddr >> REGION_STRIPE_SHIFT) & (FCRAM_STRIPE_COUNT - 1);
    }

public:
    PageLock() = default;

    class ScopedReadLock {
    public:
        ScopedReadLock(PageLock& owner, PAddr start, std::size_t size);
        ~ScopedReadLock();

        ScopedReadLock(const ScopedReadLock&) = delete;
        ScopedReadLock& operator=(const ScopedReadLock&) = delete;
        ScopedReadLock(ScopedReadLock&&) = delete;
        ScopedReadLock& operator=(ScopedReadLock&&) = delete;

    private:
        static constexpr std::size_t MAX_READ_LOCKS = 64;
        std::shared_mutex* locked[MAX_READ_LOCKS]{};
        std::size_t lock_count = 0;
    };

    class ScopedWriteLock {
    public:
        ScopedWriteLock(PageLock& owner, PAddr start, std::size_t size);
        ~ScopedWriteLock();

        ScopedWriteLock(const ScopedWriteLock&) = delete;
        ScopedWriteLock& operator=(const ScopedWriteLock&) = delete;
        ScopedWriteLock(ScopedWriteLock&&) = delete;
        ScopedWriteLock& operator=(ScopedWriteLock&&) = delete;

    private:
        static constexpr std::size_t MAX_WRITE_LOCKS = 64;
        std::shared_mutex* locked[MAX_WRITE_LOCKS]{};
        std::size_t lock_count = 0;
    };

    ScopedReadLock AcquireReadLock(PAddr start, std::size_t size) {
        return ScopedReadLock(*this, start, size);
    }

    ScopedWriteLock AcquireWriteLock(PAddr start, std::size_t size) {
        return ScopedWriteLock(*this, start, size);
    }
};

inline PageLock::ScopedReadLock::ScopedReadLock(PageLock& owner, PAddr start, std::size_t size) {
    const auto* info = owner.FindRegionInfo(start);
    if (!info)
        return;

    const PAddr end = start + static_cast<PAddr>(size);
    const std::size_t first_stripe = (start - info->base) >> PageLock::REGION_STRIPE_SHIFT;
    const std::size_t last_stripe = ((end - 1) - info->base) >> PageLock::REGION_STRIPE_SHIFT;

    lock_count = std::min(last_stripe - first_stripe + 1, MAX_READ_LOCKS);
    for (std::size_t i = 0; i < lock_count; ++i) {
        locked[i] = &info->stripes[first_stripe + i].mutex;
        locked[i]->lock_shared();
    }
}

inline PageLock::ScopedReadLock::~ScopedReadLock() {
    for (std::size_t i = 0; i < lock_count; ++i) {
        locked[i]->unlock_shared();
    }
}

inline PageLock::ScopedWriteLock::ScopedWriteLock(PageLock& owner, PAddr start, std::size_t size) {
    const auto* info = owner.FindRegionInfo(start);
    if (!info)
        return;

    const PAddr end = start + static_cast<PAddr>(size);
    const std::size_t first_stripe = (start - info->base) >> PageLock::REGION_STRIPE_SHIFT;
    const std::size_t last_stripe = ((end - 1) - info->base) >> PageLock::REGION_STRIPE_SHIFT;

    lock_count = std::min(last_stripe - first_stripe + 1, MAX_WRITE_LOCKS);
    for (std::size_t i = 0; i < lock_count; ++i) {
        locked[i] = &info->stripes[first_stripe + i].mutex;
        locked[i]->lock();
    }
}

inline PageLock::ScopedWriteLock::~ScopedWriteLock() {
    for (std::size_t i = 0; i < lock_count; ++i) {
        locked[i]->unlock();
    }
}

} // namespace Memory
