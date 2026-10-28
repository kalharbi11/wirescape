// SPDX-License-Identifier: MIT
// example.step_sequencer: 8-step pitch/gate/velocity sequencer that drives CV and note events.
// Teaching module: shows edge detection, a state machine, event output and "never strand a note-off".
#pragma once
#include "wirescape_sdk.h"

// Per-step parameter triple. Parameter IDs are API: pitch1..pitch8, gate1..gate8, vel1..vel8
// must never change once published (patches and automation refer to them by ID).
// gate: 0 = rest, 1 = note, 2 = tie (previous note keeps sounding, no new note).
#define WS_STEP(n) \
    ws::param("pitch" #n, "Pitch " #n, -2, 2, 0, "oct"), \
    ws::param("gate" #n, "Gate " #n, 0, 2, 1, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1), \
    ws::param("vel" #n, "Velocity " #n, 0, 1, 0.8f)

struct StepSequencer {
    static constexpr ws::Info info{"example.step_sequencer", "Step Sequencer", 1};

    // WS2_FIRST: if a poly signal arrives, the host hands us its first voice.
    static constexpr std::array inputs{
        ws::port("clock", "Clock", WS2_TRIGGER, WS2_FIRST),
        ws::port("reset", "Reset", WS2_TRIGGER, WS2_FIRST)};
    static constexpr uint32_t kNotes = 3;   // index of the event output below
    static constexpr std::array outputs{
        ws::port("pitch", "Pitch", WS2_PITCH),
        ws::port("gate", "Gate", WS2_GATE),
        ws::port("velocity", "Velocity", WS2_CV_UNI),
        ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};

    // 2 + 8*3 = 26 parameters (host limit is 32). Step s (0-based) lives at 2 + 3*s + {0,1,2}.
    static constexpr std::array params{
        ws::param("run", "Run", 0, 1, 1, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("length", "Length", 1, 8, 8, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        WS_STEP(1), WS_STEP(2), WS_STEP(3), WS_STEP(4), WS_STEP(5), WS_STEP(6), WS_STEP(7), WS_STEP(8)};
    static constexpr uint32_t kRun = 0, kLength = 1;
    static constexpr uint32_t pitch_param(uint32_t s) { return 2 + 3 * s; }
    static constexpr uint32_t gate_param(uint32_t s) { return 3 + 3 * s; }
    static constexpr uint32_t vel_param(uint32_t s) { return 4 + 3 * s; }

    struct Note { bool active = false; int32_t id = 0, key = 60; };

    // Playback position only. The pattern itself lives in the parameters, which the host saves.
    uint32_t step = 0;           // step played last (0-based)
    bool armed = true;           // next clock plays step 1 (set by reset)
    bool running = true;         // last observed state of the run parameter
    ws::Schmitt clock_schmitt, reset_schmitt;
    bool clock_high = false, reset_high = false;   // mirrored: Schmitt has no getter
    Note sounding;               // note whose gate is high
    Note pending;                // note-off the host could not take yet; retried, never dropped
    int32_t next_id = 0;
    int32_t last_key = 60;       // pitch/velocity outputs hold the last note after it ends
    float last_velocity = 0;

    void prepare(const ws::Setup&) {}

    // reset() runs while quiescent and cannot emit events: the host chokes notes on reset.
    // We only make sure nothing is considered sounding, so state matches what the host did.
    void reset() {
        step = 0; armed = true; running = true;
        clock_schmitt.reset(); reset_schmitt.reset();
        clock_high = reset_high = false;
        sounding = Note{}; pending = Note{};
        next_id = 0; last_key = 60; last_velocity = 0;
    }

    bool emit(const ws::Block& b, uint32_t type, uint32_t offset, const Note& n, float value) const {
        ws2_event e{};                       // zero everything, then fill the fields we use
        e.struct_size = sizeof(ws2_event);
        e.type = type;
        e.offset = offset;                   // sample index inside this block
        e.port = kNotes;
        e.note_id = n.id;
        e.channel = 0;
        e.key = n.key;
        e.value = value;
        return b.emit(kNotes, e);            // false when the bounded output queue is full
    }

    // Try to deliver a stuck note-off. At most one is ever pending, and we call this at most
    // once per decision point, so the cost per block is bounded (no retry loop).
    bool flush(const ws::Block& b, uint32_t i) {
        if (!pending.active) return true;
        if (!emit(b, WS2_NOTE_OFF, i, pending, 0.f)) return false;
        pending.active = false;
        return true;
    }

    // Gate drops immediately; the note-off follows now, or later if the queue is full.
    void release(const ws::Block& b, uint32_t i) {
        if (sounding.active) { pending = sounding; sounding.active = false; }
        flush(b, i);
    }

    void start_note(const ws::Block& b, uint32_t i, uint32_t s) {
        if (!flush(b, i)) return;            // an old note-off is still stuck: skip rather than overlap
        const long semitones = std::lround(b.param(pitch_param(s)).ch(0)[i] * 12.f);
        const int32_t key = static_cast<int32_t>(std::clamp<long>(60 + semitones, 0, 127));
        const float velocity = std::clamp(b.param(vel_param(s)).ch(0)[i], 0.f, 1.f);
        const Note n{true, next_id, key};
        if (!emit(b, WS2_NOTE_ON, i, n, velocity)) return;   // no event, no sound: stay consistent
        sounding = n;
        next_id = (next_id + 1) & 0x7fffffff;
        last_key = key;
        last_velocity = velocity;
    }

    static uint32_t length_at(const ws::Block& b, uint32_t i) {
        return static_cast<uint32_t>(std::clamp<long>(std::lround(b.param(kLength).ch(0)[i]), 1, 8));
    }
    static long gate_at(const ws::Block& b, uint32_t s, uint32_t i) {
        return std::clamp<long>(std::lround(b.param(gate_param(s)).ch(0)[i]), 0, 2);
    }
    uint32_t next_step(const ws::Block& b, uint32_t i) const { return armed ? 0 : (step + 1) % length_at(b, i); }

    void process(ws::Block& b) {
        b.out(0).set_channels(1); b.out(1).set_channels(1); b.out(2).set_channels(1); b.out(3).set_channels(0);
        for (uint32_t p = 0; p < 3; ++p) b.out(p).mark_constant(false);   // always fill: simple and exact
        float* pitch = b.out(0).ch(0);
        float* gate = b.out(1).ch(0);
        float* velocity = b.out(2).ch(0);
        const auto clock = b.in(0).ch(0);
        const auto reset_in = b.in(1).ch(0);
        const auto run = b.param(kRun).ch(0);

        flush(b, 0);                         // deliver a note-off left over from the previous block (or load_state)
        for (uint32_t i = 0; i < b.frames(); ++i) {
            // 1. Edges: reset first, then clock.
            const bool reset_now = reset_schmitt.process(reset_in[i]);
            const bool reset_rise = reset_now && !reset_high;
            reset_high = reset_now;
            const bool clock_now = clock_schmitt.process(clock[i]);
            const bool clock_rise = clock_now && !clock_high;
            const bool clock_fall = !clock_now && clock_high;
            clock_high = clock_now;

            // 2. Run switch: stopping releases the note at this sample and freezes the position.
            const bool run_now = run[i] > 0.5f;
            if (!run_now && running) release(b, i);
            running = run_now;

            // 3. Reset wins over a clock on the same sample: it only arms step 1, and the clock
            //    handled below then plays that step.
            if (reset_rise) { armed = true; release(b, i); }

            // 4. Clock.
            if (running && clock_rise) {
                step = next_step(b, i);
                armed = false;
                const long g = gate_at(b, step, i);
                if (g == 2 && sounding.active) {
                    // tie: keep the note, change nothing
                } else {
                    release(b, i);           // NOTE_OFF first (if any) ...
                    if (g == 1) start_note(b, i, step);   // ... then NOTE_ON at the same offset
                }                            // a tie with nothing sounding behaves like a rest
            } else if (clock_fall && gate_at(b, next_step(b, i), i) != 2) {
                release(b, i);               // gate ends with the clock unless the next step is a tie
            }

            // 5. CV always mirrors the note state, so CV and events cannot disagree.
            pitch[i] = static_cast<float>(last_key - 60) / 12.f;
            gate[i] = sounding.active ? 1.f : 0.f;
            velocity[i] = last_velocity;
        }
    }

    // State v1, u32 words: step, armed, running, clock_high, reset_high, note active, id, key,
    // next_id, last_key, last_velocity (f32). A pending note-off is saved as an active note.
    int save_state(ws::Writer w) {
        const Note& n = sounding.active ? sounding : pending;
        const uint32_t words[] = {1, step, armed, running, clock_high, reset_high, n.active, static_cast<uint32_t>(n.id),
                                  static_cast<uint32_t>(n.key), static_cast<uint32_t>(next_id), static_cast<uint32_t>(last_key)};
        for (uint32_t word : words) if (!w.u32(word)) return WS2_CAPACITY;
        return w.f32(last_velocity) ? WS2_OK : WS2_CAPACITY;
    }

    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;
        uint32_t s, armed_in, running_in, clock_in, reset_in, active, id, key, next, last;
        float velocity;
        if (!r.u32(s) || !r.u32(armed_in) || !r.u32(running_in) || !r.u32(clock_in) || !r.u32(reset_in) || !r.u32(active) ||
            !r.u32(id) || !r.u32(key) || !r.u32(next) || !r.u32(last) || !r.f32(velocity)) return WS2_INVALID;
        const bool sane = s < 8 && armed_in <= 1 && running_in <= 1 && clock_in <= 1 && reset_in <= 1 && active <= 1 &&
                          id <= 0x7fffffffu && key <= 127 && next <= 0x7fffffffu && last <= 127 &&
                          std::isfinite(velocity) && velocity >= 0.f && velocity <= 1.f;
        if (!sane) return WS2_INVALID;
        // Everything checked: commit. A note that was sounding is released by the next process() call,
        // because the host will not have a matching note-on for the restored session.
        step = s; armed = armed_in; running = running_in;
        clock_high = clock_in; reset_high = reset_in;
        clock_schmitt.reset(); reset_schmitt.reset();
        if (clock_high) clock_schmitt.process(1.f);
        if (reset_high) reset_schmitt.process(1.f);
        sounding = Note{};
        pending = Note{active != 0, static_cast<int32_t>(id), static_cast<int32_t>(key)};
        next_id = static_cast<int32_t>(next); last_key = static_cast<int32_t>(last); last_velocity = velocity;
        return WS2_OK;
    }
};
#undef WS_STEP
