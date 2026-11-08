// SPDX-License-Identifier: MIT
// Standalone callback test for example.step_sequencer. It drives the generated C callbacks
// with a fixed host arena and an event-output callback that records (and can refuse) events.
#include "dsp.h"
#include "../test_support.h"
#include "../../../tests2/allocation_watch.h"

using S = StepSequencer;
static constexpr uint32_t kFrames = 1024;   // max block size we prepare for
static constexpr uint32_t kParams = static_cast<uint32_t>(S::params.size());

struct Ev {
    uint32_t type; uint64_t frame; int32_t id, key; float value;
    bool operator==(const Ev& o) const { return type == o.type && frame == o.frame && id == o.id && key == o.key && value == o.value; }
};
using Events = Fixed<Ev, 2048>;
using Curve = Fixed<float, kFrames>;
struct Trace {
    Curve pitch, gate, velocity;
    Events events;
    bool operator==(const Trace& o) const {
        return pitch == o.pitch && gate == o.gate && velocity == o.velocity && events == o.events;
    }
    void append(const Trace& o) { pitch.append(o.pitch); gate.append(o.gate); velocity.append(o.velocity); events.append(o.events); }
};
template<size_t N> static bool is(const Events& events, const Ev (&expected)[N]) {
    if (events.size() != N) return false;
    for (size_t i = 0; i < N; ++i) if (!(events[i] == expected[i])) return false;
    return true;
}

// Event sink: stores events with absolute frames, refuses after `capacity` events per block.
struct Recorder {
    Fixed<Ev, 4096> events;
    uint64_t base = 0; uint32_t frames = 0, taken = 0, capacity = 1u << 30;
};
static int WS2_CALL push(void* ctx, uint32_t port, const ws2_event* e) {
    auto& r = *static_cast<Recorder*>(ctx);
    REQUIRE(port == S::kNotes && e->struct_size == sizeof(ws2_event) && e->offset < r.frames);
    REQUIRE(e->channel == 0 && e->source_id == 0 && e->bytes == 0 && e->data == nullptr);
    if (r.taken >= r.capacity) return WS2_CAPACITY;
    ++r.taken;
    r.events.push_back({e->type, r.base + e->offset, e->note_id, e->key, e->value});
    return WS2_OK;
}

// Input shapes: clock high for `high` frames every `period`; reset high for 2 frames at each start.
struct Clock { uint32_t period = 20, high = 8; float operator()(uint64_t f) const { return (f % period) < high ? 1.f : 0.f; } };
struct Resets { uint64_t at[4]{}; float operator()(uint64_t f) const { for (auto s : at) if (s && f >= s && f < s + 2) return 1.f; return 0.f; } };   // 0 = unused slot

struct Rig {
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor* d = ws::Module<S>::descriptor();
    void* state = nullptr;
    // Big sample buffers live on the heap: Windows gives a test only a 1 MB stack.
    struct Buffers { float in[2][kFrames]{}; float out[4][16][kFrames]{}; Recorder recorder; };
    std::unique_ptr<Buffers> buffers = std::make_unique<Buffers>();
    float (&in)[2][kFrames] = buffers->in;
    const float* in_rows[2]{};
    ws2_signal ins[2]{};
    float values[kParams]{};
    const float* value_rows[kParams]{};
    ws2_signal params[kParams]{};
    float (&out)[4][16][kFrames] = buffers->out;
    float* out_rows[4][16]{};
    ws2_signal_out outs[4]{};
    Recorder& recorder = buffers->recorder;
    ws2_event_out event_outs[4]{};
    ws2_process ctx{};

    explicit Rig(double rate = 48000) {
        ws2_config cfg{sizeof(cfg), rate, kFrames, 16, 42, nullptr, 0};
        state = d->create(&host);
        REQUIRE(state && d->prepare(state, &cfg, &host) == WS2_OK);
        for (uint32_t p = 0; p < kParams; ++p) {
            values[p] = S::params[p].initial;
            value_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        for (int p = 0; p < 4; ++p) {
            for (int c = 0; c < 16; ++c) out_rows[p][c] = out[p][c];
            outs[p] = {sizeof(ws2_signal_out), out_rows[p], 1, 0, 16};
            event_outs[p] = {sizeof(ws2_event_out), &recorder, static_cast<uint32_t>(p), push};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = 1;
        ctx.input_count = 2;
        ctx.output_count = 4;
        ctx.param_count = kParams;
        ctx.inputs = ins;
        ctx.params = params;
        ctx.outputs = outs;
        ctx.event_outputs = event_outs;
    }
    ~Rig() { d->destroy(state); }

    // Pattern helpers: semitone offsets from C4 and gate codes per step.
    void pattern(const int (&semitones)[8], const int (&gates)[8], uint32_t length = 8) {
        values[S::kLength] = static_cast<float>(length);
        for (uint32_t s = 0; s < 8; ++s) {
            values[S::pitch_param(s)] = static_cast<float>(semitones[s]) / 12.f;
            values[S::gate_param(s)] = static_cast<float>(gates[s]);
        }
    }

    // A named pattern prevents accidentally exchanging pitch and gate arrays.
    struct Pattern16 {
        const int (&semitones)[16];
        const int (&gates)[16];
    };
    void pattern16(Pattern16 pattern, uint32_t length = 16) {
        values[S::kLength] = static_cast<float>(length);
        for (uint32_t i = 0; i < 16; ++i) {
            values[S::pitch_param(i)] = static_cast<float>(pattern.semitones[i]) / 12.f;
            values[S::gate_param(i)] = static_cast<float>(pattern.gates[i]);
        }
    }

    int run(uint32_t frames) {
        for (int p = 0; p < 2; ++p) { in_rows[p] = in[p]; ins[p] = {sizeof(ws2_signal), &in_rows[p], 1, WS2_CONNECTED}; }
        recorder.base = ctx.frame_position; recorder.frames = frames; recorder.taken = 0;
        ctx.frames = frames;
        const int result = d->process(state, &ctx);
        ctx.frame_position += frames;
        return result;
    }

    // Render `total` frames in blocks of `block`; returns just what this call produced.
    template<class C, class R> Trace render(uint32_t total, uint32_t block, C clock, R reset) {
        Trace t;
        const size_t first_event = recorder.events.size();
        for (uint32_t done = 0; done < total;) {
            const uint32_t n = std::min(block, total - done);
            for (uint32_t i = 0; i < n; ++i) { in[0][i] = clock(ctx.frame_position + i); in[1][i] = reset(ctx.frame_position + i); }
            REQUIRE(run(n) == WS2_OK);
            for (uint32_t i = 0; i < n; ++i) { t.pitch.push_back(out[0][0][i]); t.gate.push_back(out[1][0][i]); t.velocity.push_back(out[2][0][i]); }
            done += n;
        }
        for (size_t i = first_event; i < recorder.events.size(); ++i) t.events.push_back(recorder.events[i]);
        return t;
    }
};

static const int kSemis[8] = {0, 2, 4, 5, 7, 9, 11, 12};
static const int kNotes[8] = {1, 1, 1, 1, 1, 1, 1, 1};

// Notes are monophonic: events must alternate ON, OFF with ids counting 0, 1, 2 ...
static void check_balanced(const Trace& t) {
    REQUIRE(t.events.size() % 2 == 0);
    for (size_t k = 0; k < t.events.size(); ++k) {
        REQUIRE(t.events[k].type == ((k % 2) ? WS2_NOTE_OFF : WS2_NOTE_ON));
        REQUIRE(t.events[k].id == static_cast<int32_t>(k / 2) && t.events[k].key == t.events[k - (k % 2)].key);
    }
}
// CV must agree with the events at every sample: gate = a note is sounding, pitch/velocity = that note.
static void check_agreement(const Trace& t, uint64_t first_frame = 0) {
    size_t e = 0;
    bool sounding = false; Ev note{};
    for (size_t i = 0; i < t.gate.size(); ++i) {
        for (; e < t.events.size() && t.events[e].frame == first_frame + i; ++e) {
            sounding = t.events[e].type == WS2_NOTE_ON;
            if (sounding) note = t.events[e];
        }
        REQUIRE(t.gate[i] == (sounding ? 1.f : 0.f));
        if (sounding) REQUIRE(t.pitch[i] == static_cast<float>(note.key - 60) / 12.f && t.velocity[i] == note.value);
    }
}
static Events only(const Trace& t, uint32_t type) {
    Events r;
    for (const Ev& e : t.events) if (e.type == type) r.push_back(e);
    return r;
}

static void test_advance_and_wrap() {
    Rig r;
    r.pattern(kSemis, kNotes, 3);
    r.values[S::vel_param(1)] = 0.5f;
    const Trace t = r.render(140, 64, Clock{}, Resets{});
    const int keys[7] = {60, 62, 64, 60, 62, 64, 60};
    REQUIRE(t.events.size() == 14);
    for (int k = 0; k < 7; ++k) {
        REQUIRE(t.events[2 * k].frame == 20u * k && t.events[2 * k].key == keys[k]);      // note on at the clock rise
        REQUIRE(t.events[2 * k + 1].frame == 20u * k + 8);                               // note off at the clock fall
        REQUIRE(t.events[2 * k].value == (k % 3 == 1 ? 0.5f : 0.8f));
    }
    check_balanced(t);
    check_agreement(t);
    std::puts("PASS steps advance on clock, wrap at length, CV matches events");
}

static void test_rest_and_tie() {
    Rig r;
    const int rests[8] = {1, 0, 1, 1, 1, 1, 1, 1};
    r.pattern(kSemis, rests, 4);
    Trace t = r.render(100, 100, Clock{}, Resets{});
    const auto on = only(t, WS2_NOTE_ON);
    REQUIRE(on.size() == 4 && on[0].frame == 0 && on[1].frame == 40 && on[2].frame == 60 && on[3].frame == 80);
    check_balanced(t); check_agreement(t);

    Rig tie;                                 // note, tie, note, rest
    const int ties[8] = {1, 2, 1, 0, 1, 1, 1, 1};
    const int semis[8] = {0, 7, 4, 5, 7, 9, 11, 12};
    tie.pattern(semis, ties, 4);
    t = tie.render(140, 64, Clock{}, Resets{});
    const Ev expected[] = {
        {WS2_NOTE_ON, 0, 0, 60, 0.8f}, {WS2_NOTE_OFF, 28, 0, 60, 0.f}, {WS2_NOTE_ON, 40, 1, 64, 0.8f}, {WS2_NOTE_OFF, 48, 1, 64, 0.f},
        {WS2_NOTE_ON, 80, 2, 60, 0.8f}, {WS2_NOTE_OFF, 108, 2, 60, 0.f}, {WS2_NOTE_ON, 120, 3, 64, 0.8f}, {WS2_NOTE_OFF, 128, 3, 64, 0.f}};
    REQUIRE(is(t.events, expected));           // the tie holds the note from frame 8 to 28 without a new note
    REQUIRE(t.gate[15] == 1.f && t.pitch[25] == 0.f);   // gate stays high through the tie; pitch keeps the old note
    check_balanced(t); check_agreement(t);

    Rig lonely;                              // a tie with nothing sounding is a rest
    const int none[8] = {0, 2, 0, 2, 0, 2, 0, 2};
    lonely.pattern(kSemis, none, 2);
    REQUIRE(lonely.render(100, 50, Clock{}, Resets{}).events.empty());
    std::puts("PASS rest, tie (held across fall, pitch kept), tie without a note");
}

static void test_reset_wins_and_release() {
    Rig r;
    const int gates[8] = {1, 2, 2, 1, 1, 1, 1, 1};
    r.pattern(kSemis, gates, 4);
    Trace t = r.render(100, 33, Clock{}, Resets{{40}});   // reset and clock rise on the same frame
    const Ev expected[] = {
        {WS2_NOTE_ON, 0, 0, 60, 0.8f}, {WS2_NOTE_OFF, 40, 0, 60, 0.f},   // reset releases ...
        {WS2_NOTE_ON, 40, 1, 60, 0.8f}, {WS2_NOTE_OFF, 88, 1, 60, 0.f}};  // ... and that clock plays step 1 (tie, tie, then note)
    REQUIRE(is(t.events, expected));
    check_balanced(t); check_agreement(t);

    Rig q;                                   // reset while a note sounds, clock elsewhere
    q.pattern(kSemis, kNotes, 4);
    t = q.render(100, 100, Clock{}, Resets{{25, 70}});
    REQUIRE(t.events[3].frame == 25 && t.events[3].type == WS2_NOTE_OFF);   // note from frame 20 released by reset
    REQUIRE(t.events[4].frame == 40 && t.events[4].key == 60);              // next clock restarts at step 1
    check_balanced(t); check_agreement(t);
    std::puts("PASS reset wins on the same frame, reset releases");
}

static void test_run_switch() {
    Rig r;
    r.pattern(kSemis, kNotes, 8);
    Trace a = r.render(4, 4, Clock{}, Resets{});          // note on at 0, clock still high
    r.values[S::kRun] = 0;
    Trace b = r.render(56, 56, Clock{}, Resets{});        // run=0: released at the first sample, then silence
    r.values[S::kRun] = 1;
    Trace c = r.render(80, 80, Clock{}, Resets{});        // resumes at step 2 on the frame-60 clock
    REQUIRE(a.events.size() == 1 && b.events.size() == 1);
    REQUIRE(b.events[0].type == WS2_NOTE_OFF && b.events[0].frame == 4);
    REQUIRE(c.events[0].frame == 60 && c.events[0].key == 62);
    Trace all = a;
    all.append(b);
    all.append(c);
    check_agreement(all);
    std::puts("PASS run=0 releases the note and freezes the position");
}

static void test_capacity_full() {
    Rig r;
    r.pattern(kSemis, kNotes, 8);
    r.recorder.capacity = 1;                 // the host accepts one event per block
    Trace t = r.render(200, 20, Clock{}, Resets{});       // each block: rise at offset 0, fall at offset 8
    bool delayed = false;
    for (const Ev& e : t.events) delayed = delayed || (e.type == WS2_NOTE_OFF && e.frame % 20 == 0);
    REQUIRE(delayed);                        // a refused note-off was kept and delivered at the start of a later block
    r.recorder.capacity = 1u << 30;
    t.events = r.render(20, 20, [](uint64_t) { return 0.f; }, Resets{}).events;   // clock low, room again
    Trace all;
    for (const Ev& e : r.recorder.events) all.events.push_back(e);
    check_balanced(all);                     // every note-on got its note-off, nothing stranded, no overlap
    REQUIRE(all.events.size() >= 6);
    std::puts("PASS refused note-off is kept and delivered later, notes balance");
}

static void test_matrices() {
    auto setup = [](Rig& r) {
        const int gates[8] = {1, 2, 1, 0, 1, 1, 2, 1};
        r.pattern(kSemis, gates, 7);
    };
    const Resets resets{{333, 500, 777}};
    Rig reference; setup(reference);
    const Trace whole = reference.render(1000, 1000, Clock{23, 9}, resets);
    REQUIRE(!whole.events.empty());
    for (uint32_t block : {1u, 17u, 64u, 256u}) {
        Rig r; setup(r);
        REQUIRE(r.render(1000, block, Clock{23, 9}, resets) == whole);
    }
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        Rig r(rate); setup(r);
        REQUIRE(r.render(1000, 128, Clock{23, 9}, resets).events == whole.events);   // control is frame-based
    }
    std::puts("PASS frame matrix 1/17/64/256 == one block, 44.1/48/96 kHz");
}

static void test_state() {
    const int gates[8] = {1, 2, 1, 0, 1, 1, 2, 1};
    auto make = [&](Rig& r) { r.pattern(kSemis, gates, 7); };
    auto save = [](Rig& r) { Bytes bytes; ws2_writer w{sizeof(w), &bytes, sink}; REQUIRE(r.d->save_state(r.state, &w) == WS2_OK); return bytes; };
    auto load = [](Rig& r, Bytes bytes) { Source s{&bytes, 0}; ws2_reader rd{sizeof(rd), &s, source, static_cast<uint32_t>(bytes.size())}; return r.d->load_state(r.state, &rd); };

    // Between notes (clock low, nothing sounding): restoring continues bit-identically.
    Rig a, b; make(a); make(b);
    a.render(134, 40, Clock{}, Resets{});
    REQUIRE(a.out[1][0][13] == 0.f);        // last sample of the run: gate is low
    const auto bytes = save(a);
    REQUIRE(load(b, bytes) == WS2_OK);
    b.ctx.frame_position = a.ctx.frame_position;
    REQUIRE(b.render(300, 50, Clock{}, Resets{}) == a.render(300, 50, Clock{}, Resets{}));

    // Mid-note: the restored instance releases the note, then plays the same notes as the original.
    Rig c, d2, e; make(c); make(d2); make(e);
    c.render(124, 31, Clock{}, Resets{});
    const Ev sounding = c.recorder.events.back();
    REQUIRE(sounding.type == WS2_NOTE_ON);
    REQUIRE(load(d2, save(c)) == WS2_OK);
    d2.ctx.frame_position = c.ctx.frame_position;
    const Trace tail = d2.render(100, 50, Clock{}, Resets{});
    REQUIRE(tail.events[0].type == WS2_NOTE_OFF && tail.events[0].frame == 124 && tail.events[0].id == sounding.id);
    REQUIRE(only(tail, WS2_NOTE_ON) == only(c.render(100, 50, Clock{}, Resets{}), WS2_NOTE_ON));

    // reset() clears everything: the run repeats exactly.
    const Trace first = e.render(200, 64, Clock{}, Resets{{90}});
    e.d->reset(e.state);
    e.ctx.frame_position = 0; e.recorder.events.clear();
    REQUIRE(e.render(200, 64, Clock{}, Resets{{90}}) == first);

    // Bad data: right error code, and the instance keeps its state.
    Rig f, twin; make(f); make(twin);
    f.render(50, 50, Clock{}, Resets{}); twin.render(50, 50, Clock{}, Resets{});
    auto unknown = bytes; unknown[0] = 9;
    REQUIRE(load(f, unknown) == WS2_UNSUPPORTED);
    auto truncated = bytes; truncated.pop_back();
    REQUIRE(load(f, truncated) == WS2_INVALID);
    REQUIRE(load(f, Bytes{}) == WS2_INVALID);
    auto bad_step = bytes; bad_step[4] = 17;
    REQUIRE(load(f, bad_step) == WS2_INVALID);
    auto bad_velocity = bytes; bad_velocity[bad_velocity.size() - 1] = 0x7f;      // velocity becomes NaN/huge
    REQUIRE(load(f, bad_velocity) == WS2_INVALID);
    REQUIRE(f.render(300, 64, Clock{}, Resets{}) == twin.render(300, 64, Clock{}, Resets{}));
    std::puts("PASS state round trip, mid-note load releases, reset repeats, bad state rejected");
}

static void test_no_allocation() {
    Rig r;
    r.pattern(kSemis, kNotes);
    for (uint32_t i = 0; i < 256; ++i) { r.in[0][i] = Clock{}(i); r.in[1][i] = Resets{{100}}(i); }
    allocation_watch::Scope watch;
    for (int i = 0; i < 100; ++i) {
        REQUIRE(r.run(17 + i) == WS2_OK);
        if (i % 10 == 0) r.d->reset(r.state);
    }
    REQUIRE(watch.allocations() == 0 && !r.recorder.events.empty());
    std::puts("PASS no allocation in process/reset");
}

static void test_extremes() {
    Rig r;
    for (uint32_t s = 0; s < 16; ++s) {       // out-of-range parameters the module must tolerate
        r.values[S::pitch_param(s)] = (s % 2) ? 1e6f : -1e6f;
        r.values[S::gate_param(s)] = static_cast<float>(s) - 2.f;
        r.values[S::vel_param(s)] = (s % 2) ? 9.f : -9.f;
    }
    for (float length : {-3.f, 0.f, 99.f}) {
        r.values[S::kLength] = length;
        for (uint32_t i = 0; i < 256; ++i) { r.in[0][i] = (i % 3) ? 1e30f : -1e30f; r.in[1][i] = (i % 50) ? -1e30f : 1e30f; }
        REQUIRE(r.run(256) == WS2_OK);
        for (int p = 0; p < 3; ++p) for (uint32_t i = 0; i < 256; ++i) REQUIRE(std::isfinite(r.out[p][0][i]));
    }
    for (const Ev& e : r.recorder.events) REQUIRE(e.key >= 0 && e.key <= 127 && e.value >= 0.f && e.value <= 1.f);
    std::puts("PASS finite outputs and valid events under extreme parameters");
}


static void test_sixteen_steps_and_boundary_ties() {
    static_assert(kParams == 50, "append 24 new step parameters without renumbering");
    static_assert(S::pitch_param(0) == 2 && S::vel_param(7) == 25 &&
                  S::pitch_param(8) == 26 && S::vel_param(15) == 49, "stable parameter slots");
    const int semis[16] = {0, 2, 4, 5, 7, 9, 11, 12, -12, -10, -8, -7, -5, -3, -1, 0};
    const int notes[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    Rig r;
    r.pattern16({semis, notes});
    r.values[S::vel_param(15)] = 0.37f; // exercise the final parameter slot (49)
    const Trace t = r.render(660, 64, Clock{}, Resets{});
    REQUIRE(t.events.size() == 66); // 33 notes across two cycles, matching clock falls
    for (size_t i = 0; i < 33; ++i) {
        REQUIRE(t.events[2 * i].type == WS2_NOTE_ON);
        REQUIRE(t.events[2 * i].frame == i * 20 && t.events[2 * i].key == 60 + semis[i % 16]);
        REQUIRE(t.events[2 * i].value == (i % 16 == 15 ? 0.37f : 0.8f));
        REQUIRE(t.events[2 * i + 1].type == WS2_NOTE_OFF && t.events[2 * i + 1].frame == i * 20 + 8);
    }
    check_balanced(t);
    check_agreement(t);

    // Tie over steps 8->9 and 16->1, even when the clock goes low between rises.
    const int ties[16] = {2, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1};
    Rig tied;
    tied.pattern16({semis, ties});
    const Trace hold = tied.render(360, 17, Clock{}, Resets{});
    const Ev expected[] = {
        {WS2_NOTE_ON, 140, 0, 72, 0.8f}, {WS2_NOTE_OFF, 168, 0, 72, 0.f},
        {WS2_NOTE_ON, 300, 1, 60, 0.8f}, {WS2_NOTE_OFF, 328, 1, 60, 0.f}};
    REQUIRE(is(hold.events, expected));
    REQUIRE(hold.gate[148] == 1 && hold.gate[167] == 1 && hold.gate[168] == 0);
    REQUIRE(hold.gate[308] == 1 && hold.gate[327] == 1 && hold.gate[328] == 0);
    check_balanced(hold);
    check_agreement(hold);
    // Stop while step 16 is sounding: release immediately. Restart and reset on
    // the same clock edge: the legacy reset-wins rule must still play step 1.
    Rig stopped;
    stopped.pattern16({semis, notes});
    Trace all = stopped.render(304, 17, Clock{}, Resets{});
    REQUIRE(all.events.back().type == WS2_NOTE_ON && all.events.back().frame == 300);
    stopped.values[S::kRun] = 0.f;
    const Trace off = stopped.render(16, 16, Clock{}, Resets{});
    REQUIRE(off.events.size() == 1 && off.events[0].type == WS2_NOTE_OFF && off.events[0].frame == 304);
    stopped.values[S::kRun] = 1.f;
    const Trace resumed = stopped.render(60, 17, Clock{}, Resets{{340}});
    bool step_one_on_reset = false;
    for (const Ev& e : resumed.events) {
        if (e.type == WS2_NOTE_ON && e.frame == 340 && e.key == 60) step_one_on_reset = true;
    }
    REQUIRE(step_one_on_reset);
    all.append(off);
    all.append(resumed);
    check_balanced(all);
    check_agreement(all);
    std::puts("PASS 16 steps and ties over 8/9, 16/1 with stable note events");
}

static void test_sixteen_block_matrix_and_length_edits() {
    const int semis[16] = {0, 2, 4, 5, 7, 9, 11, 12, -12, -10, -8, -7, -5, -3, -1, 0};
    const int gates[16] = {1, 1, 2, 0, 1, 1, 0, 1, 1, 1, 2, 1, 0, 1, 1, 1};
    const Resets resets{{333, 499}};
    Rig one;
    one.pattern16({semis, gates});
    const Trace expected = one.render(640, 640, Clock{23, 9}, resets);
    for (uint32_t size : {1u, 17u, 128u, 256u}) {
        Rig r;
        r.pattern16({semis, gates});
        REQUIRE(r.render(640, size, Clock{23, 9}, resets) == expected);
    }
    for (double rate : {44100., 48000., 96000.}) {
        Rig r(rate);
        r.pattern16({semis, gates});
        REQUIRE(r.render(640, 128, Clock{23, 9}, resets).events == expected.events);
    }

    Rig edit;
    edit.pattern16({semis, gates});
    edit.render(225, 17, Clock{}, Resets{}); // 12th step was just played at frame 220
    edit.values[S::kLength] = 4;
    const Trace shortened = edit.render(95, 17, Clock{}, Resets{});
    // The old note may still release after the length edit; only NEW onsets
    // must come from the retained steps, never from the removed upper bank.
    const Events new_notes = only(shortened, WS2_NOTE_ON);
    REQUIRE(!new_notes.empty());
    for (const Ev& e : new_notes) REQUIRE(e.key >= 60 && e.key <= 65);
    REQUIRE(shortened.gate.size() == 95);
    std::puts("PASS 16-step block/rate matrix and live length shortening");
}

static void test_v1_v2_state_migration() {
    const int semis[16] = {0, 2, 4, 5, 7, 9, 11, 12, -12, -10, -8, -7, -5, -3, -1, 0};
    const int gates[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    auto save = [](Rig& r) {
        Bytes data;
        ws2_writer w{sizeof(w), &data, sink};
        REQUIRE(r.d->save_state(r.state, &w) == WS2_OK);
        return data;
    };
    auto load = [](Rig& r, Bytes& data) {
        Source source_state{&data, 0};
        ws2_reader rd{sizeof(rd), &source_state, source, static_cast<uint32_t>(data.size())};
        return r.d->load_state(r.state, &rd);
    };
    Rig initial, resumed;
    initial.pattern16({semis, gates});
    resumed.pattern16({semis, gates});
    initial.render(205, 17, Clock{}, Resets{}); // step 11 at frame 200
    Bytes state2 = save(initial);
    REQUIRE(state2[0] == 2);
    REQUIRE(load(resumed, state2) == WS2_OK);
    resumed.ctx.frame_position = initial.ctx.frame_position;
    const Trace a = initial.render(155, 17, Clock{}, Resets{});
    const Trace b = resumed.render(155, 17, Clock{}, Resets{});
    REQUIRE(only(a, WS2_NOTE_ON) == only(b, WS2_NOTE_ON));
    // A v1 header must not claim an impossible >=8 saved position.
    Bytes wrong_legacy = state2;
    wrong_legacy[0] = 1;
    REQUIRE(load(resumed, wrong_legacy) == WS2_INVALID);
    // A true v1 state stays loadable and produces the same subsequent trace.
    Rig old, upgraded;
    old.pattern(kSemis, kNotes, 8);
    upgraded.pattern(kSemis, kNotes, 8);
    old.render(65, 17, Clock{}, Resets{});
    Bytes legacy = save(old);
    legacy[0] = 1;
    REQUIRE(load(upgraded, legacy) == WS2_OK);
    upgraded.ctx.frame_position = old.ctx.frame_position;
    REQUIRE(only(old.render(115, 17, Clock{}, Resets{}), WS2_NOTE_ON) ==
            only(upgraded.render(115, 17, Clock{}, Resets{}), WS2_NOTE_ON));
    std::puts("PASS v1 positions 0..7 load and v2 positions 0..15 round trip");
}

int main() {
    test_advance_and_wrap();
    test_sixteen_steps_and_boundary_ties();
    test_sixteen_block_matrix_and_length_edits();
    test_v1_v2_state_migration();
    test_rest_and_tie();
    test_reset_wins_and_release();
    test_run_switch();
    test_capacity_full();
    test_matrices();
    test_state();
    test_no_allocation();
    test_extremes();
    std::puts("step_sequencer tests passed");
}
