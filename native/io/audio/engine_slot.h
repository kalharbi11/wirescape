// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include <atomic>
#include <chrono>
#include <thread>
namespace wirescape::audio {
// Hands a prepared engine to a running render thread without stopping it.
// The render side does one relaxed load per block and, only when an engine is
// pending, one exchange: no allocation, lock, free or wait. The control side
// owns every engine and destroys the old one only after the render thread has
// adopted the new one at a block boundary, so the old one is no longer touched.
class EngineSlot {
    std::atomic<AppEngine *> pending_{nullptr};
    static_assert(std::atomic<AppEngine *>::is_always_lock_free, "engine handoff must be lock-free");

  public:
    // Render thread, at the top of each block.
    AppEngine *adopt(AppEngine *current) noexcept {
        if (!pending_.load(std::memory_order_relaxed)) return current;
        AppEngine *next = pending_.exchange(nullptr, std::memory_order_acq_rel);
        return next ? next : current;
    }
    // Control thread. True once the render thread has adopted `next` before
    // `deadline`. False means it never will: the offer was withdrawn and the
    // render thread still runs the previous engine.
    bool offer(AppEngine *next, std::chrono::steady_clock::time_point deadline) noexcept {
        pending_.store(next, std::memory_order_release);
        while (pending_.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                AppEngine *expected = next;
                // Losing this race means the render thread adopted it just now.
                return !pending_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
        return true;
    }
};
} // namespace wirescape::audio
