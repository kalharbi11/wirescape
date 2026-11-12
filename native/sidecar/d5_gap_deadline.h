// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <chrono>
#include <cstdint>

namespace wirescape::sidecar {
// The live graph first fades in the render callback, not at request dispatch.
// Called on control thread with a nonzero steady-clock timestamp from render.
using D5Clock = std::chrono::steady_clock;
inline D5Clock::time_point d5_gap_deadline(uint64_t first_callback_ns, D5Clock::duration gap_budget) noexcept {
    const auto since_epoch = std::chrono::duration_cast<D5Clock::duration>(std::chrono::nanoseconds(first_callback_ns));
    return D5Clock::time_point(since_epoch) + gap_budget;
}
} // namespace wirescape::sidecar
