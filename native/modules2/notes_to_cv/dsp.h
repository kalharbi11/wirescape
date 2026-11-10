// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../voice/note_to_cv.h"
#include "wirescape_sdk.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace notes_to_cv {

// A Notes-event counterpart to midi.to_cv. Input channel is a note's MIDI
// channel, not its allocated poly output lane. Voice ownership belongs to the
// existing NoteToCv/Allocator code (including anonymous IDs and steals).
struct NotesToCv {
    static constexpr ws::Info info{"essentials.notes_to_cv", "Notes to CV", 1};
    static constexpr std::array inputs{ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array outputs{ws::port("pitch", "Pitch", WS2_PITCH),
                                        ws::port("gate", "Gate", WS2_GATE),
                                        ws::port("velocity", "Velocity", WS2_CV_UNI)};
    static constexpr std::array params{
        ws::param("voices", "Voices", 1, 16, 4, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1)};

    wirescape::voice::NoteToCv core;
    uint32_t max_channels = 16;
    int configured_voices = 0;

    void prepare(const ws::Setup &s) { max_channels = s.max_channels(); }
    void reset() {
        core.reset();
        configured_voices = 0;
    }
    void process(ws::Block &b) {
        const int desired = std::clamp(static_cast<int>(std::lround(b.param(0).value(0, 0))), 1,
                                       static_cast<int>(max_channels));
        if (desired != configured_voices) {
            wirescape::voice::Config cfg{};
            cfg.max_voices = static_cast<uint8_t>(desired);
            cfg.mode = wirescape::voice::Mode::Rotate;
            core.configure(cfg);
            configured_voices = desired;
        }
        const uint32_t n = static_cast<uint32_t>(core.allocator().voice_count());
        for (uint32_t p = 0; p < 3; ++p) {
            b.out(p).set_channels(n);
            b.out(p).mark_constant(false);
        }
        b.split(
            [&](uint32_t first, uint32_t end) {
                for (uint32_t i = first; i < end; ++i) {
                    for (uint32_t c = 0; c < n; ++c) {
                        const auto &v = core.allocator().voice(static_cast<int>(c));
                        b.out(0).ch(c)[i] = v.pitch;
                        // A stolen/retriggered voice drops gate for one sample,
                        // allowing the downstream envelope to retrigger.
                        b.out(1).ch(c)[i] = v.gate && !(v.trigger && v.retrigger) ? 1.f : 0.f;
                        b.out(2).ch(c)[i] = v.velocity;
                    }
                    core.allocator().clear_triggers();
                }
            },
            [&](const ws2_event &e) {
                if (e.type == WS2_NOTE_ON || e.type == WS2_NOTE_OFF || e.type == WS2_NOTE_CHOKE ||
                    e.type == WS2_NOTE_EXPRESSION) {
                    // NotesToCv has signal outputs only. The allocator handles
                    // releases and expressions internally; it does not emit here.
                    core.event(e, [](const ws2_event &) {});
                }
            });
    }
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;
        reset(); // Active notes never persist into a restored patch.
        return WS2_OK;
    }
};

} // namespace notes_to_cv
