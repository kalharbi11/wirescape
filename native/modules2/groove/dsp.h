// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef emit
// Qt and cppcheck may define emit as a macro; restore the caller's definition.
#pragma push_macro("emit")
#undef emit
#define GROOVE_RESTORE_EMIT
#endif
#include "config.h"
#include "wirescape_sdk.h"
#include <array>
#include <cstdint>

namespace groove {
struct Sequencer {
    static constexpr ws::Info info{"sequencing.groove", "Groove", 2};
    static constexpr std::array<ws2_port, 0> inputs{};
    // S0 t1 slots are permanent. t3/t4 slots are reserved but not declared.
    static constexpr std::array outputs{
        ws::port("t1_pitch", "T1 Pitch", WS2_PITCH), ws::port("t1_gate", "T1 Gate", WS2_GATE),
        ws::port("t1_mod1", "T1 Mod 1", WS2_CV_BI), ws::port("t1_mod2", "T1 Mod 2", WS2_CV_BI),
        ws::port("t1_notes", "T1 Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT),
        ws::port("t2_pitch", "T2 Pitch", WS2_PITCH), ws::port("t2_gate", "T2 Gate", WS2_GATE),
        ws::port("t2_mod1", "T2 Mod 1", WS2_CV_BI), ws::port("t2_mod2", "T2 Mod 2", WS2_CV_BI),
        ws::port("t2_notes", "T2 Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    // All controls are rising-edge requests, including the published S0 run ID.
    static constexpr std::array params{
        ws::param("run", "Start", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("stop", "Stop", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("reset", "Reset", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("mute", "Mute", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("t1_run", "T1 Start", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("t1_stop", "T1 Stop", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("t1_mute", "T1 Mute", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("t2_run", "T2 Start", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("t2_stop", "T2 Stop", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("t2_mute", "T2 Mute", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1)};
    static constexpr uint32_t kTracks = 2, kSteps = 32, kParams = 10;
    struct NoteState {
        bool on = false;
        int32_t id = 0, key = 60;
    };
    struct Player {
        bool armed = true, muted = false, playing = false, enter = false, stop = false;
        NoteState sounding{}, pending{};
        float pitch = 0.f, mod1 = 0.f, mod2 = 0.f;
    };

    Config pattern{};
    std::array<Player, kTracks> players{};
    std::array<bool, kParams> observed{}, control_high{};
    std::array<bool, kTracks> mod1_active{}, mod2_active{};
    double samples_per_step = 6000., phase = 0.;
    uint32_t step = 0;
    bool running = false, stop_queued = false, muted = false, reset_wait = false;
    int32_t next_id = 1;

    bool prepare(const ws::Setup &s) {
        if (!decode(s.config.data, s.config.bytes, pattern)) return false;
        samples_per_step = s.sample_rate() * 15. / pattern.bpm;
        for (uint32_t t = 0; t < kTracks; ++t) {
            mod1_active[t] = mod2_active[t] = false;
            for (uint32_t i = 0; i < kSteps && t < pattern.track_count; ++i) {
                mod1_active[t] |= pattern.tracks[t].steps[i].mod1 != 0.f;
                mod2_active[t] |= pattern.tracks[t].steps[i].mod2 != 0.f;
            }
        }
        reset();
        return true;
    }

    void reset() {
        // Host choke handles active notes. reset() cannot emit.
        players = {};
        for (uint32_t t = 0; t < kTracks; ++t) {
            players[t].armed = t < pattern.track_count && pattern.tracks[t].armed != 0;
            players[t].muted = t < pattern.track_count && pattern.tracks[t].mute != 0;
        }
        observed.fill(false);
        control_high.fill(false);
        running = stop_queued = muted = reset_wait = false;
        phase = 0.;
        step = 0;
        next_id = 1;
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): type and sample offset are distinct concepts.
    bool emit(ws::Block &b, uint32_t t, uint32_t type, uint32_t i, NoteState n, float velocity) {
        ws2_event e{};
        e.struct_size = sizeof(e);
        e.type = type;
        e.offset = i;
        e.port = 4 + t * 5;
        e.note_id = n.id;
        e.channel = 0;
        e.key = n.key;
        e.value = velocity;
        return b.emit(e.port, e);
    }
    bool flush(ws::Block &b, uint32_t t, uint32_t i) {
        auto &p = players[t];
        if (!p.pending.on) return true;
        if (!emit(b, t, WS2_NOTE_OFF, i, p.pending, 0.f)) return false;
        p.pending = {};
        return true;
    }
    void release(ws::Block &b, uint32_t t, uint32_t i) {
        auto &p = players[t];
        if (p.sounding.on) {
            // There cannot be another pending release: a pending release blocks NOTE_ON.
            p.pending = p.sounding;
            p.sounding = {};
        }
        flush(b, t, i);
    }
    void release_all(ws::Block &b, uint32_t i) {
        for (uint32_t t = 0; t < kTracks; ++t) release(b, t, i);
    }
    bool effective_mute(uint32_t t) const { return muted || players[t].muted; }

    void trigger(ws::Block &b, uint32_t t, uint32_t i) {
        auto &p = players[t];
        if (!p.playing || effective_mute(t)) return;
        const auto &row = pattern.tracks[t].steps[step];
        if (row.kind == Tie && p.sounding.on) return;
        release(b, t, i);
        if (row.kind != Note || row.velocity <= 0.f || !flush(b, t, i)) return;
        const NoteState note{true, next_id, row.key};
        if (!emit(b, t, WS2_NOTE_ON, i, note, row.velocity)) return;
        p.sounding = note;
        next_id = next_id == 0x7fffffff ? 1 : next_id + 1;
        p.pitch = static_cast<float>(row.key - 60) / 12.f;
        p.mod1 = row.mod1;
        p.mod2 = row.mod2;
    }

    bool edge(const ws::Block &b, uint32_t p, uint32_t i) {
        const bool high = b.param(p).value(0, i) > 0.5f;
        const bool rise = observed[p] && high && !control_high[p];
        observed[p] = true;
        control_high[p] = high;
        return rise;
    }
    void boundary(ws::Block &b, uint32_t i) {
        if (stop_queued) {
            release_all(b, i);
            running = stop_queued = reset_wait = false;
            for (auto &p : players) {
                p.playing = p.enter = p.stop = false;
            }
            return; // STOP at the boundary has no extra step-1 NOTE_ON.
        }
        for (uint32_t t = 0; t < kTracks; ++t) {
            auto &p = players[t];
            if (p.stop) {
                release(b, t, i);
                p.playing = p.stop = p.enter = false;
            } else if (p.enter) {
                p.playing = true;
                p.enter = false;
            }
        }
    }
    void process(ws::Block &b) {
        for (uint32_t t = 0; t < kTracks; ++t) {
            const uint32_t base = t * 5;
            for (uint32_t p = 0; p < 4; ++p) {
                b.out(base + p).set_channels(1);
                const bool constant = t >= pattern.track_count ||
                    (p == 2 && !mod1_active[t]) || (p == 3 && !mod2_active[t]);
                b.out(base + p).mark_constant(constant);
            }
            b.out(base + 4).set_channels(0);
        }
        for (uint32_t i = 0; i < b.frames(); ++i) {
            for (uint32_t t = 0; t < kTracks; ++t) flush(b, t, i);
            std::array<bool, kParams> rise{};
            for (uint32_t p = 0; p < kParams; ++p) rise[p] = edge(b, p, i);
            bool started = false;
            if (rise[2]) { // RESET > STOP > MUTE > START > step advance.
                release_all(b, i);
                phase = 0.;
                step = 0;
                reset_wait = running;
                stop_queued = false;
                for (auto &p : players) {
                    p.enter = p.stop = false;
                    p.playing = running && p.armed;
                }
            } else {
                if (rise[1] && running) stop_queued = !stop_queued;
                for (uint32_t t = 0; t < kTracks; ++t) {
                    auto &p = players[t];
                    if (!rise[5 + t * 3] || t >= pattern.track_count) continue;
                    if (p.enter) p.enter = false; // STOP cancels a queued entry.
                    else if (running && p.playing) p.stop = !p.stop;
                    else if (!running) p.armed = false;
                }
                if (rise[3]) {
                    muted = !muted;
                    if (muted) release_all(b, i);
                }
                for (uint32_t t = 0; t < kTracks; ++t) {
                    if (t >= pattern.track_count || !rise[6 + t * 3]) continue;
                    players[t].muted = !players[t].muted;
                    if (effective_mute(t)) release(b, t, i);
                }
                if (rise[0]) {
                    if (running) stop_queued = false;
                    else {
                        running = true;
                        stop_queued = reset_wait = false;
                        phase = 0.;
                        step = 0;
                        for (uint32_t t = 0; t < kTracks; ++t) {
                            auto &p = players[t];
                            p.playing = t < pattern.track_count && p.armed;
                            p.enter = p.stop = false;
                            trigger(b, t, i);
                        }
                        started = true;
                    }
                }
                for (uint32_t t = 0; t < kTracks; ++t) {
                    if (t >= pattern.track_count || !rise[4 + t * 3]) continue;
                    auto &p = players[t];
                    p.armed = true;
                    if (!running) continue;
                    if (p.playing) p.stop = false;
                    else p.enter = true;
                }
                if (running && !started) {
                    phase += 1.;
                    if (phase >= samples_per_step) {
                        phase -= samples_per_step;
                        if (reset_wait) {
                            step = 0;
                            reset_wait = false;
                        } else {
                            step = (step + 1) % kSteps;
                            if (step == 0) boundary(b, i);
                        }
                        if (running)
                            for (uint32_t t = 0; t < kTracks; ++t) trigger(b, t, i);
                    }
                }
            }
            for (uint32_t t = 0; t < kTracks; ++t) {
                const auto &p = players[t];
                const uint32_t base = t * 5;
                b.out(base).ch(0)[i] = p.pitch;
                b.out(base + 1).ch(0)[i] = p.sounding.on ? 1.f : 0.f;
                b.out(base + 2).ch(0)[i] = p.mod1;
                b.out(base + 3).ch(0)[i] = p.mod2;
            }
        }
    }
};
} // namespace groove
#ifdef GROOVE_RESTORE_EMIT
#pragma pop_macro("emit")
#undef GROOVE_RESTORE_EMIT
#endif
