// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "config.h"
#include "wirescape_sdk.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace wirescape::osc {
struct OscOut {
    static constexpr ws::Info info{"osc.out", "OSC Out", 1};
    static constexpr std::array inputs{
        ws::port("row1", "Row 1", WS2_CV_BI, WS2_FIRST),
        ws::port("row2", "Row 2", WS2_CV_BI, WS2_FIRST),
        ws::port("row3", "Row 3", WS2_CV_BI, WS2_FIRST),
        ws::port("row4", "Row 4", WS2_CV_BI, WS2_FIRST),
        ws::port("row5", "Row 5", WS2_CV_BI, WS2_FIRST),
        ws::port("row6", "Row 6", WS2_CV_BI, WS2_FIRST),
        ws::port("row7", "Row 7", WS2_CV_BI, WS2_FIRST),
        ws::port("row8", "Row 8", WS2_CV_BI, WS2_FIRST),
        ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array<ws2_port, 0> outputs{};
    static constexpr std::array<ws2_param, 0> params{};

    OutConfiguration config{};
    OutBus local_bus{};
    OutBus *bus = &local_bus;
    std::array<bool, 8> high{};

    bool prepare(const ws::Setup &setup) {
        if (!out_configuration(setup.config.data, setup.config.bytes, config)) return false;
        bus = config.bus ? config.bus : &local_bus;
        return true;
    }
    void reset() { high.fill(false); } // Consumer owns shared bus counters and ring tails.

    void process(ws::Block &block) {
        for (uint32_t i = 0; i < 8; ++i) {
            if (!config.rows[i].enabled) continue;
            const auto signal = block.in(i).ch(0); // FIRST poly policy: only voice 0
            const bool constant = block.in(i).constant();
            const uint32_t frames = block.frames();
            const float last = signal[constant ? 0 : frames - 1];
            uint32_t rises = 0, falls = 0;
            if (config.rows[i].kind == 1) {
                bool was = high[i];
                for (uint32_t s = 0; s < frames; ++s) {
                    const bool now = signal[constant ? 0 : s] > .5f;
                    if (now != was) {
                        if (now) ++rises;
                        else ++falls;
                        was = now;
                    }
                }
                high[i] = was;
            }
            auto &row = bus->rows[i];
            row.value.store(last, std::memory_order_relaxed);
            if (rises) row.rises.fetch_add(rises, std::memory_order_relaxed);
            if (falls) row.falls.fetch_add(falls, std::memory_order_relaxed);
            row.blocks.fetch_add(1, std::memory_order_release);
        }
        if (!config.notes.enabled) return;
        for (const auto &event : block.events()) {
            if (event.type != WS2_NOTE_ON && event.type != WS2_NOTE_OFF &&
                event.type != WS2_NOTE_CHOKE) continue;
            const uint32_t head = bus->note_head.load(std::memory_order_relaxed);
            const uint32_t tail = bus->note_tail.load(std::memory_order_acquire);
            if (head - tail >= bus->notes.size()) {
                bus->dropped.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            bus->notes[head % bus->notes.size()] = OutNote{
                static_cast<int16_t>(event.key), static_cast<int16_t>(event.channel),
                event.type == WS2_NOTE_ON ? event.value : 0.f};
            bus->note_head.store(head + 1, std::memory_order_release);
        }
    }
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;
        reset();
        return WS2_OK;
    }
};
} // namespace wirescape::osc
