// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <chrono>
#include <cstdint>

namespace wirescape::sidecar {
// The live graph first fades in the render callback, not at request dispatch.
// Called on control thread with a nonzero steady-clock timestamp from render.
inline std::chrono::steady_clock::time_point d5_gap_deadline(
    uint64_t first_callback_ns, std::chrono::steady_clock::duration gap_budget) noexcept {
    using Clock = std::chrono::steady_clock;
    return Clock::time_point(std::chrono::duration_cast<Clock::duration>(
                                 std::chrono::nanoseconds(first_callback_ns))) +
           gap_budget;
}
} // namespace wirescape::sidecar
