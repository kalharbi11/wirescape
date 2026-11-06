// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include "wirescape/output_policy.h"
#include <algorithm>
#include <atomic>

namespace wirescape::audio {
// Internal callback seam: scratch has two preallocated capacity-frame rows.
// now_ns is the same steady-clock domain as MIDI SystemMonotonic timestamps.
inline bool render_callback_block(AppEngine* engine, float* scratch, uint32_t capacity,
                                  float* out, unsigned long frames, uint64_t now_ns,
                                  std::atomic<uint64_t>& faults) noexcept {
    if (!out || frames > capacity) {
        faults.fetch_add(1, std::memory_order_relaxed);
        if (out) std::fill_n(out, frames * 2, 0.f);
        return false;
    }
    float* rows[]{scratch, scratch + capacity};
    if (app_engine_render_live(engine, rows, 2, uint32_t(frames), now_ns) != WS_OK) {
        std::fill_n(out, frames * 2, 0.f);
        faults.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    for (unsigned long i = 0; i < frames; ++i) {
        out[2*i] = std::clamp(rows[0][i] * WS_OUTPUT_GAIN, -1.f, 1.f);
        out[2*i+1] = std::clamp(rows[1][i] * WS_OUTPUT_GAIN, -1.f, 1.f);
    }
    return true;
}
}
