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
        ws::port("t1_pitch", "T1 Pitch", WS2_PITCH),
        ws::port("t1_gate", "T1 Gate", WS2_GATE),
        ws::port("t1_mod1", "T1 Mod 1", WS2_CV_BI),
        ws::port("t1_mod2", "T1 Mod 2", WS2_CV_BI),
        ws::port("t1_notes", "T1 Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT),
        ws::port("t2_pitch", "T2 Pitch", WS2_PITCH),
        ws::port("t2_gate", "T2 Gate", WS2_GATE),
        ws::port("t2_mod1", "T2 Mod 1", WS2_CV_BI),
        ws::port("t2_mod2", "T2 Mod 2", WS2_CV_BI),
        ws::port("t2_notes", "T2 Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT),
        ws::port("t1_pos", "T1 Position", WS2_CV_BI),
        ws::port("t2_pos", "T2 Position", WS2_CV_BI),
        ws::port("t1_loop_lo", "T1 Loop Low", WS2_CV_BI),
        ws::port("t2_loop_lo", "T2 Loop Low", WS2_CV_BI),
        ws::port("t1_fired", "T1 Fired", WS2_GATE),
        ws::port("t2_fired", "T2 Fired", WS2_GATE),
        ws::port("t1_loop_hi", "T1 Loop High", WS2_CV_BI),
        ws::port("t2_loop_hi", "T2 Loop High", WS2_CV_BI)};
    // All controls are rising-edge requests, including the published S0 run ID.
    static constexpr std::array params{
        ws::param("run", "Start", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("stop", "Stop", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 1, 4, 1),
        ws::param("reset", "Reset", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 2, 4, 1),
        ws::param("mute", "Mute", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 3, 4, 1),
        ws::param("t1_run", "T1 Start", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 4, 4, 1),
        ws::param("t1_stop", "T1 Stop", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 5, 4, 1),
        ws::param("t1_mute", "T1 Mute", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 6, 4, 1),
        ws::param("t2_run", "T2 Start", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 7, 4, 1),
        ws::param("t2_stop", "T2 Stop", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 8, 4, 1),
        ws::param("t2_mute", "T2 Mute", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 9, 4, 1)};
    static constexpr uint32_t kTracks = 2, kSteps = 64, kParams = 10;
    struct NoteState {
        bool on = false;
        int32_t id = 0, key = 60;
    };
    struct Player {
        bool armed = true, muted = false, playing = false, enter = false, stop = false;
        NoteState sounding{}, pending{};
        float pitch = 0.f, mod1 = 0.f, mod2 = 0.f;
        uint32_t loop = 1;
        uint32_t rng = 1;
        bool fired = false;
        double gate_age = 0., gate_cutoff = 0.;
        bool short_gate = false;
    };

    Config pattern{};
    std::array<Player, kTracks> players{};
    std::array<bool, kParams> observed{}, control_high{};
    std::array<bool, kTracks> mod1_active{}, mod2_active{};
    double samples_per_step = 6000., phase = 0.;
    uint32_t step = 0;
    std::array<uint32_t, kTracks> step_t{};
    uint32_t rng_seed = 1;
    bool running = false, stop_queued = false, muted = false, reset_wait = false;
    int32_t next_id = 1;

    bool prepare(const ws::Setup &s) {
        if (!decode(s.config.data, s.config.bytes, pattern)) return false;
        samples_per_step = s.sample_rate() * 15. / pattern.bpm;
        // The quiescent adapter carries the graph's stable numeric instance ID.
        // Mixing the optional host seed adds repeatability without pointer or clock entropy.
        rng_seed = pattern.instance_seed ^ static_cast<uint32_t>(s.config.seed) ^
                   static_cast<uint32_t>(s.config.seed >> 32u);
        for (uint32_t t = 0; t < kTracks; ++t) {
            mod1_active[t] = mod2_active[t] = false;
            for (uint32_t i = 0; i < pattern.tracks[t].length && t < pattern.track_count; ++i) {
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
        for (uint32_t t = 0; t < kTracks; ++t) players[t].rng = seed_for(t);
        observed.fill(false);
        control_high.fill(false);
        step_t.fill(0);
        running = stop_queued = muted = reset_wait = false;
        phase = 0.;
        step = 0;
        next_id = 1;
    }

    // Quiescent module-state envelope; never part of authored config JSON.
    // State v1 predates native per-track loop/RNG persistence.
    int save_state(ws::Writer writer) const {
        if (!writer.u32(2)) return WS2_CAPACITY;
        for (const auto &p : players)
            if (!writer.u32(p.loop) || !writer.u32(p.rng)) return WS2_CAPACITY;
        return WS2_OK;
    }
    int load_state(ws::Reader reader) {
        uint32_t version = 0;
        if (!reader.u32(version)) return WS2_INVALID;
        if (version == 1) {
            for (uint32_t t = 0; t < kTracks; ++t) {
                players[t].loop = 1;
                players[t].rng = seed_for(t);
            }
            return WS2_OK;
        }
        if (version != 2) return WS2_INVALID;
        std::array<uint32_t, kTracks> loops{}, rngs{};
        for (uint32_t t = 0; t < kTracks; ++t)
            if (!reader.u32(loops[t]) || !reader.u32(rngs[t]) ||
                loops[t] == 0 || rngs[t] == 0) return WS2_INVALID;
        for (uint32_t t = 0; t < kTracks; ++t) {
            players[t].loop = loops[t];
            players[t].rng = rngs[t];
        }
        return WS2_OK;
    }

    uint32_t seed_for(uint32_t track) const {
        uint32_t x = rng_seed ^ (0x9e3779b9u * (track + 1u));
        x ^= x >> 16u; x *= 0x7feb352du;
        x ^= x >> 15u; x *= 0x846ca68bu;
        x ^= x >> 16u;
        return x ? x : 1u; // xorshift32 cannot start at zero.
    }
    static uint32_t next_random(uint32_t &state) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return state;
    }
    bool condition_passes(const Step &row, Player &player) {
        switch (row.cond_type) {
        case Always: return true;
        case Chance: {
            const uint32_t sample = next_random(player.rng);
            return static_cast<uint64_t>(sample) * 100u <
                   static_cast<uint64_t>(row.cond_a) * (uint64_t{1} << 32);
        }
        case AB: return (player.loop - 1u) % row.cond_b == row.cond_a - 1u;
        case First: return player.loop == 1u;
        case NotFirst: return player.loop != 1u;
        default: return false; // validated on the control thread.
        }
    }
    uint32_t master_steps() const {
        uint32_t longest = 0;
        for (uint32_t t = 0; t < pattern.track_count; ++t) {
            const auto &player = players[t];
            if (player.playing || player.enter || player.armed)
                if (pattern.tracks[t].length > longest) longest = pattern.tracks[t].length;
        }
        // A running transport with all tracks disarmed must still have a defined cycle.
        if (!longest)
            for (uint32_t t = 0; t < pattern.track_count; ++t)
                if (pattern.tracks[t].length > longest) longest = pattern.tracks[t].length;
        return longest;
    }
    double gate_duration(uint32_t track, uint32_t note_step, bool &short_gate) const {
        const auto &pattern_track = pattern.tracks[track];
        uint32_t last_gate = pattern_track.steps[note_step].gate_q;
        uint32_t ties = 0;
        for (uint32_t next = (note_step + 1u) % pattern_track.length;
             ties + 1u < pattern_track.length &&
             pattern_track.steps[next].kind == Tie;
             next = (next + 1u) % pattern_track.length) {
            ++ties;
            last_gate = pattern_track.steps[next].gate_q;
        }
        short_gate = last_gate < 4; // Full-length gates use the legacy boundary release.
        return samples_per_step * (static_cast<double>(ties) + last_gate / 4.);
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

    void release_step(ws::Block &b, uint32_t t, uint32_t i) {
        const auto &p = players[t];
        if (!p.playing || effective_mute(t)) return;
        const auto &row = pattern.tracks[t].steps[step_t[t]];
        if (row.kind != Tie || !p.sounding.on) release(b, t, i);
    }
    void trigger(ws::Block &b, uint32_t t, uint32_t i) {
        auto &p = players[t];
        const auto &row = pattern.tracks[t].steps[step_t[t]];
        const bool continued = p.fired && p.sounding.on;
        p.fired = false;
        if (!p.playing) return;
        if (row.kind == Note && row.velocity > 0.f) {
            p.fired = condition_passes(row, p);
            if (!p.fired) {
                // A failed condition acts as a rest for notes/ties, not for the
                // stepped CV modulation: the skipped trig still updates both mods.
                p.mod1 = row.mod1;
                p.mod2 = row.mod2;
                return;
            }
        } else if (row.kind == Tie) {
            p.fired = continued;
        }
        if (effective_mute(t)) return;
        if (row.kind == Tie && p.sounding.on) return;
        if (row.kind != Note || row.velocity <= 0.f || !p.fired || !flush(b, t, i)) return;
        const NoteState note{true, next_id, row.key};
        if (!emit(b, t, WS2_NOTE_ON, i, note, row.velocity)) return;
        p.sounding = note;
        p.gate_age = 0.;
        p.gate_cutoff = gate_duration(t, step_t[t], p.short_gate);
        next_id = next_id == 0x7fffffff ? 1 : next_id + 1;
        p.pitch = static_cast<float>(row.key - 60) / 12.f;
        p.mod1 = row.mod1;
        p.mod2 = row.mod2;
    }

    void trigger_all(ws::Block &b, uint32_t i) {
        for (uint32_t t = 0; t < kTracks; ++t) release_step(b, t, i);
        for (uint32_t t = 0; t < kTracks; ++t) trigger(b, t, i);
    }
    bool edge(const ws::Block &b, uint32_t p, uint32_t i) {
        const bool high = b.param(p).value(0, i) > 0.5f;
        const bool rise = observed[p] && high && !control_high[p];
        observed[p] = true;
        control_high[p] = high;
        return rise;
    }
    // One sixteenth-note tick. Track wraps are independent, while global STOP
    // waits for the longest armed/running track to realign with the master.
    void boundary(ws::Block &b, uint32_t i) {
        step = (step + 1u) % master_steps();
        for (uint32_t t = 0; t < kTracks; ++t) {
            step_t[t] = (step_t[t] + 1u) % pattern.tracks[t].length;
            auto &p = players[t];
            if (p.playing && step_t[t] == 0) ++p.loop;
        }
        if (step == 0 && stop_queued) {
            release_all(b, i);
            running = stop_queued = reset_wait = false;
            for (auto &p : players) p.playing = p.enter = p.stop = p.fired = false;
            return; // No step-1 NOTE_ON after the completed global STOP.
        }
        for (uint32_t t = 0; t < kTracks; ++t) {
            auto &p = players[t];
            if (p.stop && step_t[t] == 0) {
                release(b, t, i);
                p.playing = p.armed = p.stop = p.enter = p.fired = false;
            } else if (p.enter && step % 16u == 0) {
                p.playing = true;
                p.enter = false;
                p.loop = 1;
                p.rng = seed_for(t);
            }
        }
    }
    void process(ws::Block &b) {
        for (uint32_t t = 0; t < kTracks; ++t) {
            const uint32_t base = t * 5;
            for (uint32_t p = 0; p < 4; ++p) {
                b.out(base + p).set_channels(1);
                const bool constant =
                    t >= pattern.track_count || (p == 2 && !mod1_active[t]) || (p == 3 && !mod2_active[t]);
                b.out(base + p).mark_constant(constant);
            }
            b.out(base + 4).set_channels(0);
            for (uint32_t port : {10 + t, 12 + t, 14 + t, 16 + t}) {
                b.out(port).set_channels(1);
                b.out(port).mark_constant(false);
            }
        }
        for (uint32_t i = 0; i < b.frames(); ++i) {
            for (uint32_t t = 0; t < kTracks; ++t) {
                auto &p = players[t];
                // Only shortened final gates expire between step boundaries.
                // A full gate, including every legacy v1 pattern, continues to
                // use the original step boundary for byte-identical release.
                if (p.sounding.on && p.short_gate && p.gate_age >= p.gate_cutoff)
                    release(b, t, i);
                flush(b, t, i);
            }
            std::array<bool, kParams> rise{};
            for (uint32_t p = 0; p < kParams; ++p) rise[p] = edge(b, p, i);
            bool started = false;
            if (rise[2]) { // RESET > STOP > MUTE > START > step advance.
                release_all(b, i);
                phase = 0.;
                step = 0;
                step_t.fill(0);
                reset_wait = running;
                stop_queued = false;
                for (auto &p : players) {
                    p.enter = p.stop = p.fired = false;
                    p.loop = 1;
                    p.rng = seed_for(static_cast<uint32_t>(&p - players.data()));
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
                // Within START precedence, apply track arming before global START.
                // A simultaneous STOP always wins over START at its scope.
                for (uint32_t t = 0; t < kTracks; ++t) {
                    if (t >= pattern.track_count || !rise[4 + t * 3] || rise[5 + t * 3]) continue;
                    auto &p = players[t];
                    p.armed = true;
                    if (!running) continue;
                    if (p.playing) p.stop = false;
                    else p.enter = true;
                }
                if (rise[0] && !rise[1]) {
                    if (running) stop_queued = false;
                    else {
                        running = true;
                        stop_queued = reset_wait = false;
                        phase = 0.;
                        step = 0;
                        step_t.fill(0);
                        for (uint32_t t = 0; t < kTracks; ++t) {
                            auto &p = players[t];
                            p.playing = t < pattern.track_count && p.armed;
                            p.enter = p.stop = p.fired = false;
                            p.loop = 1;
                            p.rng = seed_for(t);
                        }
                        trigger_all(b, i);
                        started = true;
                    }
                }
                if (running && !started) {
                    phase += 1.;
                    if (phase >= samples_per_step) {
                        phase -= samples_per_step;
                        if (reset_wait) {
                            step = 0;
                            step_t.fill(0);
                            reset_wait = false;
                        } else {
                            boundary(b, i);
                        }
                        if (running) trigger_all(b, i);
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
                // STOP is complete only after the global transport, this track and
                // every owed release have stopped. A disarmed track under a running
                // transport is not stopped; a refused NOTE_OFF keeps the position.
                const bool active = running || p.playing || p.pending.on || p.sounding.on;
                b.out(10 + t).ch(0)[i] = active ? static_cast<float>(step_t[t]) : -1.f;
                b.out(12 + t).ch(0)[i] = static_cast<float>(p.loop & 0xffffu);
                b.out(14 + t).ch(0)[i] = p.fired ? 1.f : 0.f;
                b.out(16 + t).ch(0)[i] = static_cast<float>(p.loop >> 16u);
                if (p.sounding.on) players[t].gate_age += 1.;
            }
        }
    }
};
} // namespace groove
#ifdef GROOVE_RESTORE_EMIT
#pragma pop_macro("emit")
#undef GROOVE_RESTORE_EMIT
#endif
