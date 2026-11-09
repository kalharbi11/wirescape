// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#if defined(__cppcheck__)
// cppcheck's Qt library defines emit even for non-Qt translation units.
// Keep the SDK event method visible to analysis; QT_NO_KEYWORDS only affects Qt headers.
#undef emit
#endif
#include "config.h"
#include "wirescape_sdk.h"
#include <array>
#include <cstdint>
namespace groove {
// S0: immutable structured config and one eight-step track; S1 is separate.
struct Sequencer {
    static constexpr ws::Info info{"sequencing.groove", "Groove (S0)", 1};
    static constexpr std::array<ws2_port, 0> inputs{};
    static constexpr std::array outputs{
        ws::port("t1_pitch", "T1 Pitch", WS2_PITCH), ws::port("t1_gate", "T1 Gate", WS2_GATE),
        ws::port("t1_mod1", "T1 Mod 1", WS2_CV_BI), ws::port("t1_mod2", "T1 Mod 2", WS2_CV_BI),
        ws::port("t1_notes", "T1 Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array params{
        ws::param("run", "Run (edge)", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1)};
    Config pattern{};
    double samples_per_step = 6000., phase = 0.;
    uint32_t step = 0;
    bool observed = false, run_high = false, running = false;
    struct Active {
        bool on = false;
        int32_t id = 0, key = 60;
    };
    Active sounding{}, pending{};
    int32_t next_id = 1;
    float pitch = 0.f;
    bool prepare(const ws::Setup &s) {
        if (!decode(s.config.data, s.config.bytes, pattern)) return false;
        samples_per_step = s.sample_rate() * 15. / pattern.bpm;
        reset();
        return true;
    }
    void reset() {
        // Host chokes on reset; no note events may be emitted here.
        observed = run_high = running = false;
        step = 0;
        phase = 0.;
        sounding = pending = {};
        pitch = 0.f;
        next_id = 1;
    }
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): event type, then frame.
    bool emit(ws::Block &b, uint32_t type, uint32_t i, Active n, float velocity) {
        ws2_event e{};
        e.struct_size = sizeof(e);
        e.type = type;
        e.offset = i;
        e.port = 4;
        e.note_id = n.id;
        e.channel = 0;
        e.key = n.key;
        e.value = velocity;
        return b.emit(4, e);
    }
    bool flush(ws::Block &b, uint32_t i) {
        if (!pending.on) return true;
        if (!emit(b, WS2_NOTE_OFF, i, pending, 0.f)) return false;
        pending = {};
        return true;
    }
    void release(ws::Block &b, uint32_t i) {
        if (sounding.on) {
            pending = sounding;
            sounding = {};
        }
        flush(b, i);
    }
    void trigger(ws::Block &b, uint32_t i) {
        const auto &row = pattern.steps[step];
        if (row.kind == Tie && sounding.on) return;
        release(b, i);
        if (row.kind != Note || row.velocity <= 0.f || !flush(b, i)) return;
        const Active candidate{true, next_id, row.key};
        if (!emit(b, WS2_NOTE_ON, i, candidate, row.velocity)) return;
        sounding = candidate;
        next_id = (next_id + 1) & 0x7fffffff;
        pitch = static_cast<float>(row.key - 60) / 12.f;
    }
    void process(ws::Block &b) {
        for (uint32_t p = 0; p < 4; ++p) {
            b.out(p).set_channels(1);
            b.out(p).mark_constant(false);
        }
        b.out(4).set_channels(0);
        for (uint32_t i = 0; i < b.frames(); ++i) {
            flush(b, i);
            const bool high = b.param(0).value(0, i) > 0.5f;
            bool started = false;
            if (!observed) {
                // A saved run=1 must never cause a load-time note-on.
                observed = true;
                run_high = high;
            } else {
                if (!high && run_high) {
                    running = false;
                    release(b, i);
                } else if (high && !run_high) {
                    release(b, i);
                    running = true;
                    step = 0;
                    phase = 0.;
                    trigger(b, i);
                    started = true;
                }
                run_high = high;
            }
            if (running && !started) {
                phase += 1.;
                if (phase >= samples_per_step) {
                    phase -= samples_per_step;
                    step = (step + 1) % pattern.count;
                    trigger(b, i);
                }
            }
            b.out(0).ch(0)[i] = pitch;
            b.out(1).ch(0)[i] = sounding.on ? 1.f : 0.f;
            b.out(2).ch(0)[i] = 0.f;
            b.out(3).ch(0)[i] = 0.f;
        }
    }
};
} // namespace groove
