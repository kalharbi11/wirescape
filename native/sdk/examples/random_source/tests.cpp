// SPDX-License-Identifier: MIT
// Standalone callback test for example.random_source. It drives the generated C callbacks
// (create/prepare/process/...) with a fixed host arena, like a real host would.
#include "dsp.h"
#include "../test_support.h"
#include "../../../tests2/allocation_watch.h"

static constexpr uint32_t kFrames = 1024;   // max block size we prepare for

// One module instance plus every buffer a process call needs.
struct Rig {
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor* d = ws::Module<RandomSource>::descriptor();
    void* state = nullptr;
    // Big sample buffers live on the heap: Windows gives a test only a 1 MB stack.
    struct Buffers { float clock[16][kFrames]{}; float out[2][16][kFrames]{}; };
    std::unique_ptr<Buffers> buffers = std::make_unique<Buffers>();
    float (&clock)[16][kFrames] = buffers->clock;
    const float* clock_rows[16]{};
    ws2_signal clock_signal{};
    float values[3]{1, 0.05f, 1};               // voices, smooth, range (plain units)
    const float* value_rows[3]{};
    ws2_signal params[3]{};
    float (&out)[2][16][kFrames] = buffers->out;
    float* out_rows[2][16]{};
    ws2_signal_out outs[2]{};
    ws2_process ctx{};
    uint32_t flags = WS2_CONNECTED;             // clock flags: add WS2_CONSTANT for a constant clock

    explicit Rig(double rate = 48000, uint64_t seed = 42, float voices = 1) {
        values[0] = voices;
        ws2_config cfg{sizeof(cfg), rate, kFrames, 16, seed, nullptr, 0};
        state = d->create(&host);
        REQUIRE(state && d->prepare(state, &cfg, &host) == WS2_OK);
        for (uint32_t c = 0; c < 16; ++c) {
            clock_rows[c] = clock[c];
            for (int p = 0; p < 2; ++p) out_rows[p][c] = out[p][c];
        }
        for (int p = 0; p < 3; ++p) {
            value_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        for (int p = 0; p < 2; ++p) outs[p] = {sizeof(ws2_signal_out), out_rows[p], 1, 0, 16};
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = 1;
        ctx.input_count = 1;
        ctx.output_count = 2;
        ctx.param_count = 3;
        ctx.inputs = &clock_signal;
        ctx.params = params;
        ctx.outputs = outs;
    }
    ~Rig() { d->destroy(state); }
    int run(uint32_t frames) {
        clock_signal = {sizeof(ws2_signal), clock_rows, 1, flags};
        ctx.frames = frames;
        const int result = d->process(state, &ctx);
        ctx.frame_position += frames;
        return result;
    }
    uint32_t voices() const { return outs[0].channels; }
};

// Pulse train: high for 5 frames of every 23 (prime, so it drifts against block sizes).
static float pulse(uint64_t frame) { return (frame % 23) < 5 ? 1.f : 0.f; }

// Render `total` frames in blocks of `block`. Result layout is [output][voice][frame], constants expanded.
using Samples = Fixed<float, 32768>;
static Samples render(Rig& r, uint32_t total, uint32_t block, uint64_t start = 0) {
    Samples result;
    result.resize(32 * total);
    for (uint32_t done = 0; done < total;) {
        const uint32_t n = std::min(block, total - done);
        for (uint32_t i = 0; i < n; ++i) r.clock[0][i] = pulse(start + done + i);
        REQUIRE(r.run(n) == WS2_OK);
        for (uint32_t p = 0; p < 2; ++p)
            for (uint32_t c = 0; c < r.voices(); ++c)
                for (uint32_t i = 0; i < n; ++i)
                    result[(p * 16 + c) * total + done + i] = r.out[p][c][(r.outs[p].flags & WS2_CONSTANT) ? 0 : i];
        done += n;
    }
    return result;
}
static bool same(const Samples& a, const Samples& b) { return a == b; }

static void test_random_accessor() {
    ws::Random a;
    a.seed(7, 3);
    for (int i = 0; i < 10; ++i) a.next();
    const ws::Random::State saved = a.save();
    float expected[8];
    for (float& x : expected) x = a.bipolar();
    ws::Random b;
    b.restore(saved);                       // a different object continues the same stream
    for (float x : expected) REQUIRE(b.bipolar() == x);
    REQUIRE(saved.sequence % 2 == 1);
    std::puts("PASS Random save/restore");
}

static void test_frame_matrix() {
    Rig reference(48000, 42, 4);
    const auto whole = render(reference, 1000, 1000);
    for (uint32_t block : {1u, 17u, 64u, 256u}) {
        Rig r(48000, 42, 4);
        REQUIRE(same(render(r, 1000, block), whole));
    }
    std::puts("PASS frame matrix 1/17/64/256 == one block");
}

static void test_rate_and_smooth_time() {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        Rig r(rate);
        r.values[1] = 0.01f;
        for (uint32_t i = 0; i < kFrames; ++i) r.clock[0][i] = i == 0 ? 1.f : 0.f;   // one edge at sample 0
        REQUIRE(r.run(kFrames) == WS2_OK);
        const float target = r.out[0][0][0];
        REQUIRE(std::abs(target) > 0.05f);
        const uint32_t n = static_cast<uint32_t>(0.01 * rate);                     // t seconds in samples
        const double fraction = r.out[1][0][n - 1] / target;
        REQUIRE(std::abs(fraction - (1.0 - std::exp(-1.0))) < 0.01);               // 63.2% at t
    }
    std::puts("PASS 44.1/48/96 kHz, smooth reaches 63% at its time");
}

static void test_channels_and_fan_out() {
    for (float voices : {1.f, 4.f, 16.f}) {
        Rig r(48000, 42, voices);
        render(r, 200, 200);
        REQUIRE(r.outs[0].channels == static_cast<uint32_t>(voices) && r.outs[1].channels == static_cast<uint32_t>(voices));
        if (voices == 1) continue;
        // Mono clock reaches every voice: values change at the same frames but differ per voice.
        bool differ = false;
        for (uint32_t c = 1; c < voices; ++c) {
            differ = differ || r.out[0][c][10] != r.out[0][0][10];
            for (uint32_t i = 1; i < 200; ++i)
                REQUIRE((r.out[0][c][i] != r.out[0][c][i - 1]) == (r.out[0][0][i] != r.out[0][0][i - 1]));
        }
        REQUIRE(differ);
    }
    std::puts("PASS voices 1/4/16, mono clock fans out, voices differ");
}

static void test_determinism() {
    Rig a(48000, 42, 2), b(48000, 42, 2), c(48000, 43, 2);
    const auto ra = render(a, 300, 64), rb = render(b, 300, 64), rc = render(c, 300, 64);
    REQUIRE(same(ra, rb));
    REQUIRE(!same(ra, rc));
    a.d->reset(a.state);                    // reset restarts the same sequence
    REQUIRE(same(render(a, 300, 64, 0), ra));
    std::puts("PASS same seed identical, different seed differs, reset repeats");
}

static void test_constant() {
    Rig r(48000, 42);
    r.flags = WS2_CONNECTED | WS2_CONSTANT;
    r.values[1] = 0.002f;                   // fast smoothing so it converges in a few blocks
    Rig general(48000, 42);
    general.values[1] = 0.002f;
    std::fill_n(r.clock[0], 256, 1.f);      // constant high clock: one edge at sample 0 of block one
    std::fill_n(general.clock[0], 256, 1.f);
    bool smooth_became_constant = false;
    for (int block = 0; block < 40; ++block) {
        for (uint32_t c = 0; c < 2; ++c) std::fill_n(r.out[c][0], 256, 12345.f);   // poison
        REQUIRE(r.run(256) == WS2_OK && general.run(256) == WS2_OK);
        REQUIRE(r.outs[0].flags & WS2_CONSTANT);                  // step: constant from the first block
        REQUIRE(r.out[0][0][1] == 12345.f);                       // ...and only one sample written
        for (uint32_t i = 0; i < 256; ++i) {                      // equals the general (non-constant) path
            REQUIRE(r.out[0][0][(r.outs[0].flags & WS2_CONSTANT) ? 0 : i] == general.out[0][0][i]);
            REQUIRE(r.out[1][0][(r.outs[1].flags & WS2_CONSTANT) ? 0 : i] == general.out[1][0][i]);
        }
        if (block == 0) REQUIRE(!(r.outs[1].flags & WS2_CONSTANT));       // smooth still gliding
        if (r.outs[1].flags & WS2_CONSTANT) {
            smooth_became_constant = true;
            REQUIRE(r.out[1][0][0] == r.out[0][0][0]);                   // converged exactly to step
            REQUIRE(r.out[1][0][1] == 12345.f);
        }
    }
    REQUIRE(smooth_became_constant);
    REQUIRE(r.outs[1].flags & WS2_CONSTANT);
    // Silent constant clock: nothing ever fires, both outputs stay constant zero.
    Rig quiet(48000, 42);
    quiet.flags = WS2_CONSTANT;
    REQUIRE(quiet.run(64) == WS2_OK && (quiet.outs[0].flags & quiet.outs[1].flags & WS2_CONSTANT) && quiet.out[0][0][0] == 0);
    std::puts("PASS CONSTANT clock gives CONSTANT step (1 sample), smooth settles, equals general path");
}

static void test_state() {
    Rig a(48000, 42, 3), fresh(48000, 99, 3);
    render(a, 333, 64);                     // stop mid-pulse so Schmitt, held and smooth are all non-trivial
    Bytes bytes;
    ws2_writer writer{sizeof(writer), &bytes, sink};
    REQUIRE(a.d->save_state(a.state, &writer) == WS2_OK);
    auto load_into = [](Rig& rig, Bytes data) {
        Source s{&data, 0};
        ws2_reader reader{sizeof(reader), &s, source, static_cast<uint32_t>(data.size())};
        return rig.d->load_state(rig.state, &reader);
    };
    REQUIRE(load_into(fresh, bytes) == WS2_OK);
    REQUIRE(same(render(fresh, 400, 50, 333), render(a, 400, 50, 333)));   // loaded == uninterrupted

    // Bad data must fail with the right code and leave the instance untouched.
    Rig c(48000, 42, 3), twin(48000, 42, 3);
    render(c, 100, 100);
    render(twin, 100, 100);
    auto unknown = bytes;
    unknown[0] = 2;                                             // version 2
    REQUIRE(load_into(c, unknown) == WS2_UNSUPPORTED);
    auto truncated = bytes;
    truncated.resize(truncated.size() - 4);
    REQUIRE(load_into(c, truncated) == WS2_INVALID);
    REQUIRE(load_into(c, Bytes{}) == WS2_INVALID);
    auto bad_flag = bytes;
    bad_flag[4 + 4 * 4 + 4 + 8] = 7;                            // voice 0 Schmitt flag
    REQUIRE(load_into(c, bad_flag) == WS2_INVALID);
    auto even_stream = bytes;
    even_stream[4 + 8] &= 0xfe;                                 // voice 0 stream increment made even
    REQUIRE(load_into(c, even_stream) == WS2_INVALID);
    REQUIRE(same(render(c, 200, 64, 100), render(twin, 200, 64, 100)));
    std::puts("PASS state round trip, UNSUPPORTED/INVALID leave state unchanged");
}

static void test_no_allocation() {
    Rig r(48000, 42, 16);
    for (uint32_t i = 0; i < 256; ++i) r.clock[0][i] = pulse(i);
    allocation_watch::Scope watch;
    for (int i = 0; i < 64; ++i) {
        REQUIRE(r.run(17 + i) == WS2_OK);
        if (i % 8 == 0) r.d->reset(r.state);
    }
    REQUIRE(watch.allocations() == 0);
    std::puts("PASS no allocation in process/reset");
}

static void test_extremes() {
    Rig r(96000, 1, 16);
    for (float voices : {0.f, 1.f, 16.f, 99.f, -3.f})
        for (float smooth : {-1.f, 0.f, 1e-9f, 1.f, 100.f})
            for (float range : {0.f, 1.f, 1e6f}) {
                r.values[0] = voices; r.values[1] = smooth; r.values[2] = range;
                for (uint32_t i = 0; i < 256; ++i) r.clock[0][i] = (i % 2) ? 1e30f : -1e30f;   // extreme clock
                REQUIRE(r.run(256) == WS2_OK);
                REQUIRE(r.voices() >= 1 && r.voices() <= 16);
                for (int p = 0; p < 2; ++p)
                    for (uint32_t c = 0; c < r.voices(); ++c)
                        for (uint32_t i = 0; i < 256; ++i) REQUIRE(std::isfinite(r.out[p][c][i]));
            }
    std::puts("PASS finite outputs under extreme parameters");
}

int main() {
    test_random_accessor();
    test_frame_matrix();
    test_rate_and_smooth_time();
    test_channels_and_fan_out();
    test_determinism();
    test_constant();
    test_state();
    test_no_allocation();
    test_extremes();
    std::puts("random_source tests passed");
}
