// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace wirescape::osc {
struct OutRow {                               // render writes, sender reads
    std::atomic<float> value{0};              // channel 0, last sample of the latest block
    std::atomic<uint32_t> rises{0}, falls{0}; // gate rows: monotonic crossing counters (wrap ok)
    std::atomic<uint32_t> blocks{0};          // bumped after value/rises/falls are stored (release)
};
struct OutNote {
    int16_t key = 0;
    int16_t channel = 0;
    float velocity = 0;
}; // velocity 0 = off
struct OutBus {
    std::array<OutRow, 8> rows{};
    std::array<OutNote, 64> notes{};
    std::atomic<uint32_t> note_head{0}, note_tail{0}; // SPSC: render pushes head, sender pops tail
    std::atomic<uint32_t> dropped{0};                 // ring full on the render side
};
} // namespace wirescape::osc
