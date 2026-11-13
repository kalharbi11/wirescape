// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <chrono>
#include <cstdint>

namespace wirescape::sidecar {
// The null sink's Windows waitable timer takes a *relative*, negative count of
// 100 ns ticks. Round upward: arming early must never move a render ahead of
// the absolute steady-clock deadline used by the strict overrun accounting.
inline int64_t sink_due_100ns(std::chrono::steady_clock::time_point now,
                              std::chrono::steady_clock::time_point deadline) noexcept {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
    if (ns <= 0) return 0;
    return -(ns / 100 + (ns % 100 != 0));
}

enum class SinkWaitResult : std::uint8_t { deadline, stopped, unavailable };

// Injectable clock and wait primitive make early timer signals, missed periods
// and shutdown deterministic in native tests; the real Windows backend uses
// SetWaitableTimer/WaitForMultipleObjects and the monotonic steady clock.
template <class Now, class Wait>
SinkWaitResult sink_wait_until(std::chrono::steady_clock::time_point deadline, Now now, Wait wait) {
    // A broken timer must not busy-spin indefinitely on early signals.
    // After bounded retries, the caller falls back to the interruptible CV.
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const int64_t due = sink_due_100ns(now(), deadline);
        if (!due) return SinkWaitResult::deadline;
        const auto result = wait(due);
        if (result != SinkWaitResult::deadline) return result;
    }
    return SinkWaitResult::unavailable;
}
} // namespace wirescape::sidecar
