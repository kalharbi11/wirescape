// SPDX-License-Identifier: GPL-3.0-only
// Callback-level tests for essentials.quantizer. Reference math below is independent of dsp.h:
// plain interval lists and a brute-force nearest-note search.
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <vector>

using quantizer::Quantizer;
static constexpr uint32_t kFrames = 1024;
enum : uint8_t { kScale, kRoot, kShift, kOctave, kNotes }; // parameter slots; notes n0..n11 start at kNotes

struct Rig {
    struct VoiceCount {
        uint32_t value;
    };
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor *d = ws::Module<Quantizer>::descriptor();
    void *state = nullptr;
    struct Buffers {
        float in[2][16][kFrames]{};
        float out[2][16][kFrames]{};
    };
    std::unique_ptr<Buffers> buffers = std::make_unique<Buffers>();
    float (&in)[2][16][kFrames] = buffers->in;   // [pitch|trigger][voice][frame]
    float (&out)[2][16][kFrames] = buffers->out; // [pitch|changed][voice][frame]
    const float *in_rows[2][16]{};
    float *out_rows[2][16]{};
    ws2_signal inputs[2]{};
    float values[16]{1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
    const float *value_rows[16]{};
    ws2_signal params[16]{};
    ws2_signal_out outs[2]{};
    ws2_process ctx{};
    uint32_t in_channels[2]{1, 1};
    uint32_t in_flags[2]{WS2_CONNECTED, 0}; // trigger starts unconnected
    uint32_t voices = 1;

    explicit Rig(double rate = 48000, VoiceCount voice_count = {1}) : voices(voice_count.value) {
        ws2_config cfg{sizeof(cfg), rate, kFrames, 16, 42, nullptr, 0};
        state = d->create(&host);
        REQUIRE(state && d->prepare(state, &cfg, &host) == WS2_OK);
        for (int p = 0; p < 2; ++p)
            for (uint32_t c = 0; c < 16; ++c) {
                in_rows[p][c] = in[p][c];
                out_rows[p][c] = out[p][c];
            }
        for (int p = 0; p < 16; ++p) {
            value_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        for (int p = 0; p < 2; ++p) outs[p] = {sizeof(ws2_signal_out), out_rows[p], 1, 0, 16};
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.input_count = 2;
        ctx.output_count = 2;
        ctx.param_count = 16;
        ctx.inputs = inputs;
        ctx.params = params;
        ctx.outputs = outs;
    }
    ~Rig() { d->destroy(state); }
    void connect_trigger(uint32_t channels = 1) {
        in_flags[1] = WS2_CONNECTED;
        in_channels[1] = channels;
    }
    int run(uint32_t frames) {
        for (int p = 0; p < 2; ++p) inputs[p] = {sizeof(ws2_signal), in_rows[p], in_channels[p], in_flags[p]};
        ctx.channels = voices;
        ctx.frames = frames;
        const int result = d->process(state, &ctx);
        ctx.frame_position += frames;
        return result;
    }
    float pitch(uint32_t c, uint32_t i) const { return out[0][c][(outs[0].flags & WS2_CONSTANT) ? 0 : i]; }
    float changed(uint32_t c, uint32_t i) const { return out[1][c][(outs[1].flags & WS2_CONSTANT) ? 0 : i]; }
    void set_notes(uint32_t bits) {
        for (int k = 0; k < 12; ++k) values[kNotes + k] = static_cast<float>((bits >> k) & 1u);
    }
};

// Independent scale tables as interval lists (semitones above the root), same order as the Scale parameter.
static const std::vector<std::vector<int>> kScales{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
                                                   {0, 2, 4, 5, 7, 9, 11},
                                                   {0, 2, 3, 5, 7, 8, 10},
                                                   {0, 2, 3, 5, 7, 8, 11},
                                                   {0, 2, 3, 5, 7, 9, 10},
                                                   {0, 1, 3, 5, 7, 8, 10},
                                                   {0, 2, 4, 6, 7, 9, 11},
                                                   {0, 2, 4, 5, 7, 9, 10},
                                                   {0, 1, 3, 5, 6, 8, 10},
                                                   {0, 2, 4, 7, 9},
                                                   {0, 3, 5, 7, 10},
                                                   {0, 3, 5, 6, 7, 10},
                                                   {0, 2, 4, 6, 8, 10}};

static bool in_scale(const std::vector<int> &intervals, int root, int note) {
    const int rel = ((note - root) % 12 + 12) % 12;
    for (int i : intervals)
        if (i == rel) return true;
    return false;
}
// Brute force: scan every candidate upward, keep the strictly closer one, so ties go down.
static int brute_nearest(int root, const std::vector<int> &intervals, double x) {
    int best = 0;
    double best_distance = 1e9;
    for (int n = -80; n <= 80; ++n) {
        if (!in_scale(intervals, root, n)) continue;
        const double distance = std::abs(x - n);
        if (distance < best_distance) {
            best = n;
            best_distance = distance;
        }
    }
    return best;
}
static float pitch_of(int note) { return static_cast<float>(note / 12.0); }
static float in_pitch(double semitones) { return static_cast<float>(semitones / 12.0); }

// Feed one block of single-voice pitch values and run.
static void feed(Rig &r, const double *semitones, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) r.in[0][0][i] = in_pitch(semitones[i]);
    REQUIRE(r.run(frames) == WS2_OK);
}

static void test_sweep_all_scales() {
    // Points stay 0.15 semitone away from every midpoint (multiples of 0.5), so hysteresis cannot matter.
    std::vector<double> points;
    for (int i = 0; i <= 768; ++i) {
        const double x = -24 + i * 0.0625;
        if (std::abs(x * 2 - std::round(x * 2)) / 2 >= 0.15) points.push_back(x);
    }
    for (int scale = 0; scale < 13; ++scale)
        for (int root : {0, 3, 11}) {
            Rig r;
            r.values[kScale] = static_cast<float>(scale);
            r.values[kRoot] = static_cast<float>(root);
            for (size_t start = 0; start < points.size(); start += 200) {
                const uint32_t n = static_cast<uint32_t>(std::min<size_t>(200, points.size() - start));
                feed(r, &points[start], n);
                for (uint32_t i = 0; i < n; ++i)
                    REQUIRE(r.pitch(0, i) == pitch_of(brute_nearest(root, kScales[scale], points[start + i])));
            }
        }
    std::puts("PASS sweep -2..+2 V matches brute-force nearest for 13 scales x 3 roots");
}

static void test_ties_go_down() {
    // Exact midpoints in Major that survive the float conversion (x/12 is exact for these).
    for (double x : {3.0, 6.0, -6.0, -9.0}) {
        Rig r;
        feed(r, &x, 1);
        const int expected = brute_nearest(0, kScales[1], x);
        REQUIRE(r.pitch(0, 0) == pitch_of(expected));
        REQUIRE(expected < x); // the lower neighbour won
    }
    std::puts("PASS ties go down");
}

static void test_hysteresis() {
    Rig r; // Major: C(0) and D(2) meet at 1.0 semitone
    double x[400];
    uint32_t n = 0;
    auto noise = [](uint32_t i) { return (static_cast<int>((i * 7) % 11) - 5) / 100.0; }; // within +-0.05
    x[n++] = 0.95;                                                                        // settles on C
    for (uint32_t i = 1; i < 100; ++i) x[n++] = 0.95;
    for (uint32_t i = 0; i < 100; ++i) x[n++] = 1.0 + noise(i); // chatter at the boundary
    for (uint32_t i = 0; i < 100; ++i) x[n++] = 1.2;            // +0.2 past: moves to D
    for (uint32_t i = 0; i < 100; ++i) x[n++] = 1.0 + noise(i); // back at the boundary
    feed(r, x, 400);
    for (uint32_t i = 0; i < 200; ++i) {
        REQUIRE(r.pitch(0, i) == 0.f);
        REQUIRE(r.changed(0, i) == 0.f);
    }
    uint32_t ones = 0;
    for (uint32_t i = 200; i < 400; ++i) {
        REQUIRE(r.pitch(0, i) == pitch_of(2));
        ones += r.changed(0, i) != 0.f;
    }
    REQUIRE(r.changed(0, 199) == 0.f && r.changed(0, 200) == 1.f); // on the frame of the change
    REQUIRE(ones == 48);                                           // one 1 ms pulse, nothing else
    double down[100];
    for (double &v : down) v = 0.8; // 0.2 below the midpoint: back to C
    feed(r, down, 100);
    REQUIRE(r.pitch(0, 0) == 0.f && r.changed(0, 0) == 1.f);
    std::puts("PASS hysteresis holds +-0.05, moves at +-0.2, one change pulse");
}

static void test_trigger() {
    Rig r;
    r.values[kScale] = 0; // Chromatic
    r.connect_trigger();
    double x[300];
    for (uint32_t i = 0; i < 300; ++i) {
        x[i] = i * 0.1;
        r.in[1][0][i] = (i >= 20 && i < 25) || (i >= 200 && i < 205) ? 1.f : 0.f;
    }
    feed(r, x, 300);
    for (uint32_t i = 0; i < 300; ++i) {
        const int expected = i < 20 ? 0 : i < 200 ? 2 : 20; // first sample taken, then only on rising edges
        REQUIRE(r.pitch(0, i) == pitch_of(expected));
    }
    uint32_t ones = 0, edges = 0;
    for (uint32_t i = 0; i < 300; ++i) {
        ones += r.changed(0, i) != 0.f;
        edges += r.changed(0, i) != 0.f && (i == 0 || r.changed(0, i - 1) == 0.f);
    }
    REQUIRE(r.changed(0, 19) == 0.f && r.changed(0, 20) == 1.f && r.changed(0, 199) == 0.f && r.changed(0, 200) == 1.f);
    REQUIRE(edges == 2 && ones == 96);
    // An edge that lands on the same note fires nothing.
    Rig same;
    same.connect_trigger();
    for (uint32_t i = 0; i < 100; ++i) {
        same.in[0][0][i] = in_pitch(4.0);
        same.in[1][0][i] = (i % 20) < 5 ? 1.f : 0.f;
    }
    REQUIRE(same.run(100) == WS2_OK);
    for (uint32_t i = 0; i < 100; ++i) REQUIRE(same.changed(0, i) == 0.f && same.pitch(0, i) == pitch_of(4));
    std::puts("PASS trigger sample-and-hold and change pulse");
}

static void test_root_and_shift() {
    { // Root D, Major: D E F# G A B C#
        Rig r;
        r.values[kRoot] = 2;
        const double notes[] = {2, 4, 6, 7, 9, 11, 13};
        for (double note : notes) {
            Rig one;
            one.values[kRoot] = 2;
            feed(one, &note, 1);
            REQUIRE(one.pitch(0, 0) == pitch_of(static_cast<int>(note)));
        }
        const double off_scale[] = {3.3, 5.3, 8.3, 10.3, 12.2}; // D#, F, G#, A#, C -> neighbours in D major
        const int expected[] = {4, 6, 9, 11, 13};
        feed(r, off_scale, 5);
        for (int i = 0; i < 5; ++i) REQUIRE(r.pitch(0, i) == pitch_of(expected[i]));
    }
    struct Case {
        double x;
        int shift;
        int octave;
        int expected;
    };
    for (Case k : {Case{0, 1, 0, 2}, Case{11, 1, 0, 12}, Case{0, -1, 0, -1}, Case{0, 7, 0, 12}, Case{0, -7, 0, -12},
                   Case{4, 3, 0, 9}, Case{0, 0, 1, 12}, Case{0, 1, -2, -22}, Case{0, 0, 3, 36}}) {
        Rig r;
        r.values[kShift] = static_cast<float>(k.shift);
        r.values[kOctave] = static_cast<float>(k.octave);
        feed(r, &k.x, 1);
        REQUIRE(r.pitch(0, 0) == pitch_of(k.expected));
    }
    Rig pent; // Major pentatonic C D E G A: +1 from A is C an octave up
    pent.values[kScale] = 9;
    pent.values[kShift] = 1;
    const double a = 9;
    feed(pent, &a, 1);
    REQUIRE(pent.pitch(0, 0) == pitch_of(12));
    std::puts("PASS root D major, shift +1 C->D and B->C up, wrap, octave");
}

static void test_custom() {
    Rig empty; // all off: chromatic
    empty.values[kScale] = 13;
    empty.set_notes(0);
    const double x[] = {3.3, 3.6, -4.4, 0.0};
    feed(empty, x, 4);
    for (int i = 0; i < 4; ++i) REQUIRE(empty.pitch(0, i) == pitch_of(static_cast<int>(std::lround(x[i]))));
    for (int root : {0, 5}) { // C, E, G only; the root is ignored by Custom
        Rig r;
        r.values[kScale] = 13;
        r.values[kRoot] = static_cast<float>(root);
        r.set_notes((1u << 0) | (1u << 4) | (1u << 7));
        const double y[] = {2.2, 5.9, 9.9, -3.5,
                            4.2}; // E, G, C above, G below, E (each leaves the previous hold region)
        const int expected[] = {4, 7, 12, -5, 4};
        feed(r, y, 5);
        for (int i = 0; i < 5; ++i) REQUIRE(r.pitch(0, i) == pitch_of(expected[i]));
    }
    Rig tie; // C and F# only: 3.0 is the exact midpoint, the lower note wins
    tie.values[kScale] = 13;
    tie.set_notes((1u << 0) | (1u << 6));
    const double three = 3.0;
    feed(tie, &three, 1);
    REQUIRE(tie.pitch(0, 0) == 0.f);
    std::puts("PASS custom scale, empty custom is chromatic, root ignored");
}

static void test_voices() {
    Rig r(48000, {16}); // independent voices, each with its own pitch
    r.values[kScale] = 1;
    for (uint32_t c = 0; c < 16; ++c) {
        r.in_channels[0] = 16;
        for (uint32_t i = 0; i < 8; ++i) r.in[0][c][i] = in_pitch(c * 1.7 - 12 + 0.3);
    }
    REQUIRE(r.run(8) == WS2_OK && r.outs[0].channels == 16 && r.outs[1].channels == 16);
    for (uint32_t c = 0; c < 16; ++c)
        for (uint32_t i = 0; i < 8; ++i)
            REQUIRE(r.pitch(c, i) == pitch_of(brute_nearest(0, kScales[1], c * 1.7 - 12 + 0.3)));
    Rig mono(48000, {16}); // mono pitch broadcasts to all voices
    for (uint32_t i = 0; i < 8; ++i) mono.in[0][0][i] = in_pitch(6.1);
    REQUIRE(mono.run(8) == WS2_OK && mono.outs[0].channels == 16);
    for (uint32_t c = 0; c < 16; ++c) REQUIRE(mono.pitch(c, 3) == pitch_of(7));
    Rig gate(48000, {16}); // mono trigger broadcasts: every voice updates on the same edge
    gate.connect_trigger(1);
    gate.in_channels[0] = 16;
    for (uint32_t c = 0; c < 16; ++c)
        for (uint32_t i = 0; i < 40; ++i) {
            gate.in[0][c][i] = in_pitch(i < 10 ? 0.0 : 2.0 + c * 12);
            gate.in[1][0][i] = (i >= 20 && i < 25) ? 1.f : 0.f;
        }
    REQUIRE(gate.run(40) == WS2_OK);
    for (uint32_t c = 0; c < 16; ++c) {
        REQUIRE(gate.pitch(c, 19) == 0.f && gate.pitch(c, 20) == pitch_of(2 + 12 * static_cast<int>(c)));
        REQUIRE(gate.changed(c, 20) == 1.f);
    }
    std::puts("PASS 16 voices independent, mono pitch and trigger broadcast");
}

// Deterministic pseudo-random walk plus a pulse train, for the matrix tests.
static float walk(uint32_t voice, uint64_t frame) {
    const double t = static_cast<double>(frame);
    return static_cast<float>(std::sin(t * 0.013 + voice) * 0.9 + std::sin(t * 0.171 + 2 * voice) * 0.05);
}
static float pulse(uint64_t frame) { return (frame % 37) < 6 ? 1.f : 0.f; }
using Samples = Fixed<float, 32768>;
struct RenderBlock {
    uint32_t size;
};
static Samples render(Rig &r, uint32_t total, RenderBlock block, uint64_t start = 0) {
    Samples result;
    result.resize(static_cast<size_t>(8) * total);
    for (uint32_t done = 0; done < total;) {
        const uint32_t n = std::min(block.size, total - done);
        for (uint32_t c = 0; c < 4; ++c)
            for (uint32_t i = 0; i < n; ++i) r.in[0][c][i] = walk(c, start + done + i);
        for (uint32_t i = 0; i < n; ++i) r.in[1][0][i] = pulse(start + done + i);
        REQUIRE(r.run(n) == WS2_OK);
        for (uint32_t p = 0; p < 2; ++p)
            for (uint32_t c = 0; c < 4; ++c)
                for (uint32_t i = 0; i < n; ++i)
                    result[(p * 4 + c) * total + done + i] = r.out[p][c][(r.outs[p].flags & WS2_CONSTANT) ? 0 : i];
        done += n;
    }
    return result;
}
static void test_frame_matrix() {
    for (bool gated : {false, true}) {
        Rig reference(48000, {4});
        reference.in_channels[0] = 4;
        if (gated) reference.connect_trigger();
        const auto whole = render(reference, 1000, {1000});
        for (uint32_t block : {1u, 17u, 64u, 256u}) {
            Rig r(48000, {4});
            r.in_channels[0] = 4;
            if (gated) r.connect_trigger();
            REQUIRE(render(r, 1000, {block}) == whole);
        }
    }
    std::puts("PASS frame matrix 1/17/64/256 == one block (continuous and triggered)");
}

static void test_rates() {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        Rig r(rate);
        double x[400];
        for (uint32_t i = 0; i < 400; ++i) x[i] = i < 100 ? 0.0 : 2.0;
        feed(r, x, 400);
        uint32_t ones = 0;
        for (uint32_t i = 0; i < 400; ++i) ones += r.changed(0, i) != 0.f;
        REQUIRE(ones == static_cast<uint32_t>(std::ceil(rate * 0.001))); // 1 ms pulse at every rate
        REQUIRE(r.pitch(0, 99) == 0.f && r.pitch(0, 100) == pitch_of(2));
    }
    std::puts("PASS 44.1/48/96 kHz, 1 ms change pulse");
}

static void test_reset_and_state() {
    Rig a(48000, {4}), fresh(48000, {4});
    a.in_channels[0] = fresh.in_channels[0] = 4;
    a.connect_trigger();
    fresh.connect_trigger();
    const auto first = render(a, 500, {64});
    a.d->reset(a.state);
    REQUIRE(render(a, 500, {64}) == first); // reset gives the same output as a fresh instance
    REQUIRE(render(fresh, 500, {64}) == first);
    Bytes bytes;
    ws2_writer writer{sizeof(writer), &bytes, sink};
    REQUIRE(a.d->save_state(a.state, &writer) == WS2_UNSUPPORTED); // held notes are not state
    std::puts("PASS reset determinism, no saved state");
}

static void test_constant() {
    Rig c(48000), g(48000);
    c.in_flags[0] = WS2_CONNECTED | WS2_CONSTANT;
    for (Rig *r : {&c, &g})
        for (uint32_t i = 0; i < 64; ++i) r->in[0][0][i] = in_pitch(1.2);
    const float values[] = {1.2f, 1.2f, 1.8f, 1.8f, 1.8f, 3.4f, 3.4f};
    for (float semitones : values) {
        for (Rig *r : {&c, &g})
            for (uint32_t i = 0; i < 64; ++i) r->in[0][0][i] = in_pitch(semitones);
        REQUIRE(c.run(64) == WS2_OK && g.run(64) == WS2_OK);
        REQUIRE(c.outs[0].flags & WS2_CONSTANT); // pitch is constant for constant input
        for (uint32_t i = 0; i < 64; ++i) {
            REQUIRE(c.pitch(0, i) == g.pitch(0, i)); // equals the general path
            REQUIRE(c.changed(0, i) == g.changed(0, i));
        }
        const bool pulse_running = c.out[1][0][63] != 0.f || (c.outs[1].flags & WS2_CONSTANT) == 0;
        if (c.outs[1].flags & WS2_CONSTANT) REQUIRE(c.out[1][0][0] == 0.f && !pulse_running);
    }
    REQUIRE(c.outs[1].flags & WS2_CONSTANT); // quiet again after the pulses ended
    // Varying input never claims CONSTANT.
    Rig v(48000);
    for (uint32_t i = 0; i < 64; ++i) v.in[0][0][i] = in_pitch(i * 0.3);
    REQUIRE(v.run(64) == WS2_OK && !(v.outs[0].flags & WS2_CONSTANT) && !(v.outs[1].flags & WS2_CONSTANT));
    std::puts("PASS CONSTANT only for constant input, equals general path");
}

static void test_extremes() {
    Rig r(96000, {16});
    r.in_channels[0] = 16;
    r.connect_trigger(16);
    for (float scale : {-5.f, 0.f, 1.f, 13.f, 99.f})
        for (float root : {-4.f, 0.f, 11.f, 90.f})
            for (float shift : {-99.f, -7.f, 0.f, 7.f, 99.f})
                for (float octave : {-99.f, 3.f, 99.f}) {
                    r.values[kScale] = scale;
                    r.values[kRoot] = root;
                    r.values[kShift] = shift;
                    r.values[kOctave] = octave;
                    for (uint32_t c = 0; c < 16; ++c)
                        for (uint32_t i = 0; i < 64; ++i) {
                            r.in[0][c][i] = (i % 3 == 0) ? 1e30f : (i % 3 == 1) ? -1e30f : 1e-30f;
                            r.in[1][c][i] = (i % 2) ? 1e30f : -1e30f;
                        }
                    REQUIRE(r.run(64) == WS2_OK);
                    for (int p = 0; p < 2; ++p)
                        for (uint32_t c = 0; c < 16; ++c)
                            for (uint32_t i = 0; i < 64; ++i) REQUIRE(std::isfinite(r.out[p][c][i]));
                }
    std::puts("PASS finite outputs under extreme parameters and inputs");
}

static void test_no_allocation() {
    Rig r(48000, {16});
    r.in_channels[0] = 16;
    r.connect_trigger(16);
    for (uint32_t c = 0; c < 16; ++c)
        for (uint32_t i = 0; i < 256; ++i) {
            r.in[0][c][i] = walk(c, i);
            r.in[1][c][i] = pulse(i);
        }
    allocation_watch::Scope watch;
    for (int i = 0; i < 64; ++i) {
        REQUIRE(r.run(17 + i) == WS2_OK);
        if (i % 8 == 0) r.d->reset(r.state);
    }
    REQUIRE(watch.allocations() == 0);
    std::puts("PASS no allocation in process/reset");
}

int main() {
    test_sweep_all_scales();
    test_ties_go_down();
    test_hysteresis();
    test_trigger();
    test_root_and_shift();
    test_custom();
    test_voices();
    test_frame_matrix();
    test_rates();
    test_reset_and_state();
    test_constant();
    test_extremes();
    test_no_allocation();
    std::puts("quantizer tests passed");
}
