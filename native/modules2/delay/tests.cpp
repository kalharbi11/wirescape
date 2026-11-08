// SPDX-License-Identifier: GPL-3.0-only
// Callback-level tests for fx.delay. Expected values come from plain arithmetic on sample positions, not from dsp.h.
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <cmath>
#include <functional>
#include <vector>

using fxdelay::Delay;
enum : uint8_t { kTime, kDivision, kFeedback, kTone, kMode, kMix };
using Wave = std::vector<float>;

struct Arena {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(size_t(4) * 1024 * 1024);
    size_t used = 0;
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- fixed host allocator ABI.
    static void *WS2_CALL alloc(void *context, size_t bytes, size_t alignment, uint32_t) {
        auto &a = *static_cast<Arena *>(context);
        const uintptr_t base = reinterpret_cast<uintptr_t>(a.bytes.data());
        const uintptr_t next = (base + a.used + alignment - 1) & ~(uintptr_t(alignment) - 1);
        const size_t end = next - base + bytes;
        if (end > a.bytes.size()) return nullptr;
        a.used = end;
        return a.bytes.data() + (next - base);
    }
};

// One input port: `channels` waves of equal length, patched or not.
struct In {
    std::vector<Wave> ch{Wave{}};
    bool connected = false;
};
struct Result {
    Wave l, r;
};

struct Rig {
    Arena arena;
    ws2_host host{sizeof(host), &arena, Arena::alloc};
    const ws2_descriptor *d = ws::Module<Delay>::descriptor();
    void *instance = nullptr;
    double rate;
    float values[6];
    In in[3];
    Wave zeros = Wave(256, 0.f);

    explicit Rig(double sample_rate = 48000) : rate(sample_rate) {
        const ws2_config config{sizeof(config), rate, 256, 16, 7, nullptr, 0};
        instance = d->create(&host);
        REQUIRE(instance && d->prepare(instance, &config, &host) == WS2_OK);
        for (int p = 0; p < 6; ++p) values[p] = Delay::params[p].initial;
    }
    ~Rig() { d->destroy(instance); }
    Bytes save() {
        Bytes bytes;
        ws2_writer writer{sizeof(writer), &bytes, sink};
        REQUIRE(d->save_state(instance, &writer) == WS2_OK);
        return bytes;
    }
    int load(const Bytes &bytes) {
        Source ctx{&bytes, 0};
        ws2_reader reader{sizeof(reader), &ctx, source, static_cast<uint32_t>(bytes.size())};
        return d->load_state(instance, &reader);
    }
    // Runs `total` frames in blocks of `block`; `each` may change values before every block.
    Result run(size_t total, uint32_t block = 256, const std::function<void(size_t)> &each = {}, bool watch = false) {
        Result result{Wave(total), Wave(total)};
        float out_data[2][16][256]{};
        for (size_t first = 0; first < total; first += block) {
            if (each) each(first);
            const uint32_t n = static_cast<uint32_t>(std::min<size_t>(block, total - first));
            const float *rows[3][16]{};
            ws2_signal inputs[3]{};
            for (int p = 0; p < 3; ++p) {
                const In &port = in[p];
                const size_t count = port.connected ? port.ch.size() : 1;
                for (size_t c = 0; c < count; ++c)
                    rows[p][c] = port.connected ? port.ch[c].data() + first : zeros.data();
                inputs[p] = {sizeof(ws2_signal), rows[p], static_cast<uint32_t>(count),
                             port.connected ? WS2_CONNECTED : WS2_CONSTANT};
            }
            const float *value_rows[6];
            ws2_signal params[6];
            for (int p = 0; p < 6; ++p) {
                value_rows[p] = &values[p];
                params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
            }
            float *out_rows[2][16];
            ws2_signal_out outputs[2];
            for (int p = 0; p < 2; ++p) {
                for (int c = 0; c < 16; ++c) out_rows[p][c] = out_data[p][c];
                outputs[p] = {sizeof(ws2_signal_out), out_rows[p], 1, 0, 16};
            }
            ws2_process ctx{};
            ctx.struct_size = sizeof(ctx);
            ctx.sample_rate = rate;
            ctx.channels = 1;
            ctx.frames = n;
            ctx.frame_position = first;
            ctx.input_count = 3;
            ctx.output_count = 2;
            ctx.param_count = 6;
            ctx.inputs = inputs;
            ctx.outputs = outputs;
            ctx.params = params;
            int status;
            if (watch) {
                allocation_watch::Scope scope;
                status = d->process(instance, &ctx);
                REQUIRE(scope.allocations() == 0);
            } else {
                status = d->process(instance, &ctx);
            }
            REQUIRE(status == WS2_OK);
            REQUIRE(outputs[0].channels == 1 && outputs[1].channels == 1);
            REQUIRE(!(outputs[0].flags & WS2_CONSTANT) && !(outputs[1].flags & WS2_CONSTANT));
            for (uint32_t i = 0; i < n; ++i) {
                result.l[first + i] = out_data[0][0][i];
                result.r[first + i] = out_data[1][0][i];
            }
        }
        return result;
    }
    void left(Wave w) {
        in[0].ch = {std::move(w)};
        in[0].connected = true;
    }
    void right(Wave w) {
        in[1].ch = {std::move(w)};
        in[1].connected = true;
    }
    void clock(Wave w) {
        in[2].ch = {std::move(w)};
        in[2].connected = true;
    }
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- duration and onset are intentionally separate.
static Wave impulse(size_t length, size_t at = 0, float height = 1.f) {
    Wave w(length, 0.f);
    w[at] = height;
    return w;
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- sample length, seed, and amplitude are distinct.
static Wave noise(size_t length, uint32_t seed, float amount = 1.f) {
    Wave w(length);
    uint32_t s = seed;
    for (auto &x : w) {
        s = s * 1664525u + 1013904223u;
        x = amount * (static_cast<float>(s >> 8) / 8388608.f - 1.f);
    }
    return w;
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- half-open search bounds are intentional.
static size_t argmax(const Wave &w, size_t from, size_t to) {
    size_t best = from;
    for (size_t i = from; i < to; ++i)
        if (std::fabs(w[i]) > std::fabs(w[best])) best = i;
    return best;
}
static bool finite_within(const Wave &w, float limit) {
    for (float x : w)
        if (!std::isfinite(x) || std::fabs(x) > limit) return false;
    return true;
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- delay time and loop feedback are different parameters.
static void wet_only(Rig &r, float time, float feedback) {
    r.values[kMix] = 1.f;
    r.values[kTime] = time;
    r.values[kFeedback] = feedback;
}

static void echo_position() {
    for (double rate : {44100., 48000., 96000.}) {
        Rig r(rate);
        wet_only(r, 0.1f, 0.f);
        const size_t length = static_cast<size_t>(rate * 0.3), at = static_cast<size_t>(std::lround(rate * 0.1));
        r.left(impulse(length));
        const Result out = r.run(length);
        for (const Wave *w : {&out.l, &out.r}) {
            const size_t peak = argmax(*w, 0, length);
            REQUIRE(peak + 1 >= at && peak <= at + 1);
            REQUIRE(std::fabs((*w)[peak]) > 0.99f);
            for (size_t i = 0; i < length; ++i)
                if (i + 3 < at || i > at + 3) REQUIRE(std::fabs((*w)[i]) < 0.01f); // the only echo
        }
    }
}
static void feedback_decay() {
    Rig r;
    wet_only(r, 0.1f, 0.5f);
    r.left(impulse(30000));
    const Result out = r.run(30000);
    float previous = 0;
    for (int k = 1; k <= 5; ++k) {
        const size_t centre = size_t(4800) * size_t(k);
        const float peak = std::fabs(out.l[argmax(out.l, centre - 5, centre + 5)]);
        REQUIRE(std::fabs(peak - std::pow(0.5f, float(k - 1))) < 0.02f * std::pow(0.5f, float(k - 1)));
        if (k > 1) REQUIRE(std::fabs(peak / previous - 0.5f) < 0.01f);
        previous = peak;
    }
}
static void ping_pong() {
    Rig r;
    wet_only(r, 0.1f, 0.5f);
    r.values[kMode] = 1.f;
    r.left(impulse(30000));
    const Result out = r.run(30000);
    for (int k = 1; k <= 5; ++k) {
        const Wave &hit = k % 2 ? out.l : out.r; // L, R, L, R, L
        const Wave &quiet = k % 2 ? out.r : out.l;
        const size_t centre = size_t(4800) * size_t(k);
        const size_t peak = argmax(hit, centre - 5, centre + 5);
        const float expected = std::pow(0.5f, float(k - 1));
        REQUIRE(std::fabs(std::fabs(hit[peak]) - expected) < 0.02f * expected);
        REQUIRE(std::fabs(quiet[peak]) < 0.01f);
    }
    // Stereo mode with a left-only input never reaches the right output.
    Rig s;
    wet_only(s, 0.1f, 0.5f);
    s.left(impulse(12000));
    s.right(Wave(12000, 0.f));
    const Result stereo = s.run(12000);
    REQUIRE(finite_within(stereo.r, 0.01f));
    REQUIRE(std::fabs(stereo.l[argmax(stereo.l, 4795, 4805)]) > 0.99f);
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- duration and clock period are distinct.
static Wave pulses(size_t length, size_t period, size_t first = 0) {
    Wave w(length, 0.f);
    for (size_t at = first; at < length; at += period)
        for (size_t i = at; i < std::min(length, at + 100); ++i) w[i] = 1.f;
    return w;
}
static void clock_sync() {
    // 120 BPM quarter notes = 0.5 s; division 2 = x1/2 -> 0.25 s = 12000 frames, once the glide has settled.
    const size_t length = 96000 + 20000, at = 96000;
    Rig r;
    wet_only(r, 0.375f, 0.f);
    r.values[kDivision] = 2.f;
    r.left(impulse(length, at));
    r.clock(pulses(length, 24000));
    const Result out = r.run(length, 128);
    const size_t peak = argmax(out.l, at, length);
    REQUIRE(peak >= at + 12000 - 3 && peak <= at + 12000 + 3);
    REQUIRE(std::fabs(out.l[peak]) > 0.95f);
    // Dotted eighth at the same tempo: x3/4 = 0.375 s = 18000 frames (the time knob is ignored while patched).
    Rig q;
    wet_only(q, 0.1f, 0.f);
    q.values[kDivision] = 4.f;
    q.left(impulse(length, at));
    q.clock(pulses(length, 24000));
    const Result dotted = q.run(length);
    const size_t p2 = argmax(dotted.l, at, length);
    REQUIRE(p2 >= at + 18000 - 3 && p2 <= at + 18000 + 3);
    // A clock cable that never pulses leaves the time knob in charge.
    Rig idle;
    wet_only(idle, 0.1f, 0.f);
    idle.left(impulse(12000));
    idle.clock(Wave(12000, 0.f));
    const Result still = idle.run(12000);
    const size_t p3 = argmax(still.l, 0, 12000);
    REQUIRE(p3 >= 4799 && p3 <= 4801);
}
static void mix_law() {
    const size_t length = 20000;
    Rig dry;
    dry.values[kMix] = 0.f;
    dry.values[kFeedback] = 0.9f;
    dry.left(noise(length, 1));
    dry.right(noise(length, 2));
    const Result a = dry.run(length);
    // NOLINTNEXTLINE(bugprone-suspicious-memory-comparison) -- bit-exact dry passthrough includes signed zero.
    REQUIRE(std::memcmp(a.l.data(), dry.in[0].ch[0].data(), length * sizeof(float)) == 0);
    // NOLINTNEXTLINE(bugprone-suspicious-memory-comparison) -- bit-exact dry passthrough includes signed zero.
    REQUIRE(std::memcmp(a.r.data(), dry.in[1].ch[0].data(), length * sizeof(float)) == 0);
    Rig wet;
    wet_only(wet, 0.1f, 0.f);
    wet.left(noise(length, 1));
    const Result b = wet.run(length);
    for (size_t i = 0; i < 4790; ++i) REQUIRE(b.l[i] == 0.f); // no dry component
    for (size_t i = 4800; i < length; ++i) REQUIRE(std::fabs(b.l[i] - wet.in[0].ch[0][i - 4800]) < 1e-3f);
    // Equal power: a 0.5 mix of dry and the fully correlated echo has the independent gain cos+sin of 45 degrees.
    Rig half;
    wet_only(half, 0.1f, 0.f);
    half.values[kMix] = 0.5f;
    half.left(Wave(length, 0.25f));
    const Result c = half.run(length);
    REQUIRE(std::fabs(c.l[length - 1] - 0.25f * 2.f * std::sqrt(0.5f)) < 1e-3f);
}
static void unpatched_right() {
    Rig r;
    r.values[kMix] = 0.5f;
    r.left(noise(10000, 3));
    const Result out = r.run(10000);
    // NOLINTNEXTLINE(bugprone-suspicious-memory-comparison) -- left/right mirroring requires exact audio bits.
    REQUIRE(std::memcmp(out.l.data(), out.r.data(), size_t(10000) * sizeof(float)) == 0);
    REQUIRE(!finite_within(out.l, 0.f)); // not silent
}
static void poly_sum() {
    Rig one, two;
    for (Rig *r : {&one, &two}) wet_only(*r, 0.1f, 0.f);
    one.left(impulse(6000, 10, 0.5f));
    two.in[0].ch = {impulse(6000, 10, 0.25f), impulse(6000, 10, 0.25f)};
    two.in[0].connected = true;
    const Result a = one.run(6000), b = two.run(6000);
    REQUIRE(a.l == b.l && a.r == b.r);
    REQUIRE(std::fabs(a.l[argmax(a.l, 4800, 4820)] - 0.5f) < 0.01f);
}
static void loop_safety() {
    for (uint32_t mode = 0; mode < 2; ++mode) {
        Rig r;
        r.values[kFeedback] = 1.f;
        r.values[kMix] = 0.5f;
        r.values[kTime] = 0.02f;
        r.values[kMode] = static_cast<float>(mode);
        r.left(noise(480000, 5));
        r.right(noise(480000, 6));
        const Result out = r.run(480000);
        REQUIRE(finite_within(out.l, 2.f) && finite_within(out.r, 2.f));
        // The loop holds repeats up instead of dying away: late output is still substantial.
        float late = 0;
        for (size_t i = 470000; i < 480000; ++i) late = std::max(late, std::fabs(out.l[i]));
        REQUIRE(late > 0.2f);
    }
    // After the input stops, repeats at feedback 1 are sustained at a bounded level, never growing.
    Rig s;
    wet_only(s, 0.01f, 1.f);
    s.left([&] {
        Wave w = noise(4800, 8);
        w.resize(96000, 0.f);
        return w;
    }());
    const Result tail = s.run(96000);
    float last = 0;
    for (size_t i = 90000; i < 96000; ++i) last = std::max(last, std::fabs(tail.l[i]));
    REQUIRE(last > 0.05f && last <= 1.6f);
    // Sweep 0.001 -> 2 s and back while a sine plays: finite, no jump bigger than the limiter ceiling.
    Rig w;
    w.values[kFeedback] = 0.8f;
    w.values[kMix] = 0.6f;
    Wave sine(480000);
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = 0.5f * std::sin(6.2831853f * 220.f * static_cast<float>(i) / 48000.f);
    w.left(sine);
    const Result out = w.run(480000, 64, [&](size_t at) {
        const double phase = static_cast<double>(at) / 480000.0;
        const double up = phase < 0.5 ? phase * 2 : 2 - phase * 2;
        w.values[kTime] = static_cast<float>(0.001 * std::pow(2000.0, up));
    });
    REQUIRE(finite_within(out.l, 2.f));
    for (size_t i = 1; i < out.l.size(); ++i) REQUIRE(std::fabs(out.l[i] - out.l[i - 1]) <= 1.5f);
}
static void tone_shapes_loop() {
    // Dark tone loses highs in the loop; thin tone loses lows. Compare the second echo of a 100 Hz and a 6 kHz tone.
    auto second_echo = [](float tone, float hz) {
        Rig r;
        wet_only(r, 0.1f, 1.f);
        r.values[kTone] = tone;
        Wave w(40000, 0.f);
        for (size_t i = 0; i < 4800; ++i) w[i] = 0.5f * std::sin(6.2831853f * hz * static_cast<float>(i) / 48000.f);
        r.left(w);
        const Result out = r.run(40000);
        float peak = 0;
        for (size_t i = 24000; i < 33000; ++i) peak = std::max(peak, std::fabs(out.l[i]));
        return peak;
    };
    REQUIRE(second_echo(-1.f, 6000.f) < 0.25f * second_echo(0.f, 6000.f));
    REQUIRE(second_echo(-1.f, 100.f) > 0.8f * second_echo(0.f, 100.f));
    REQUIRE(second_echo(1.f, 100.f) < 0.25f * second_echo(0.f, 100.f));
    REQUIRE(second_echo(1.f, 6000.f) > 0.5f * second_echo(0.f, 6000.f)); // five passes of a gentle 1.5 kHz high-pass
}
static Wave mixed(Rig &r, uint32_t block, size_t length) {
    r.values[kTime] = 0.013f;
    r.values[kFeedback] = 0.7f;
    r.values[kTone] = -0.4f;
    r.values[kMode] = 1.f;
    r.values[kMix] = 0.5f;
    r.left(noise(length, 11, 0.6f));
    r.right(noise(length, 12, 0.6f));
    r.clock(pulses(length, 1500));
    const Result out = r.run(length, block);
    Wave both = out.l;
    both.insert(both.end(), out.r.begin(), out.r.end());
    return both;
}
static void matrices() {
    for (double rate : {44100., 48000., 96000.}) {
        Rig whole(rate);
        const Wave reference = mixed(whole, 256, 5000);
        REQUIRE(!finite_within(reference, 0.f));
        for (uint32_t block : {1u, 17u, 64u}) {
            Rig cut(rate);
            REQUIRE(mixed(cut, block, 5000) == reference);
        }
        whole.d->reset(whole.instance);
        REQUIRE(mixed(whole, 17, 5000) == reference); // reset equals a fresh instance
    }
    // Channel matrix: 1, 4 and 16 voices of 1/n each sum to the same input; a mono cable behaves the same.
    Rig base;
    wet_only(base, 0.05f, 0.3f);
    base.left(noise(8000, 4));
    const Result expected = base.run(8000);
    for (size_t voices : {size_t(4), size_t(16)}) {
        Rig poly;
        wet_only(poly, 0.05f, 0.3f);
        poly.in[0].ch.assign(voices, Wave(8000));
        for (size_t c = 0; c < voices; ++c)
            for (size_t i = 0; i < 8000; ++i) poly.in[0].ch[c][i] = base.in[0].ch[0][i] / static_cast<float>(voices);
        poly.in[0].connected = true;
        const Result got = poly.run(8000);
        for (size_t i = 0; i < 8000; ++i) REQUIRE(std::fabs(got.l[i] - expected.l[i]) < 1e-4f);
    }
}
static void state_and_extremes() {
    Rig a;
    wet_only(a, 0.01f, 0.8f);
    a.left(noise(4000, 9));
    a.run(4000);
    const Bytes bytes = a.save();
    Rig b;
    REQUIRE(b.load(bytes) == WS2_OK && b.save() == bytes);
    Bytes bad = bytes;
    bad[0] = 99;
    REQUIRE(b.load(bad) == WS2_UNSUPPORTED);
    bad = bytes;
    bad.pop_back();
    REQUIRE(b.load(bad) == WS2_INVALID);
    // Loading clears the lines: silence in, silence out afterwards.
    REQUIRE(a.load(bytes) == WS2_OK);
    a.left(Wave(4000, 0.f));
    const Result quiet = a.run(4000);
    REQUIRE(finite_within(quiet.l, 0.f) && finite_within(quiet.r, 0.f));
    // Extremes: every parameter at its minimum and maximum, hot inputs, tiny and large blocks.
    for (int extreme = 0; extreme < 2; ++extreme) {
        for (uint32_t mode = 0; mode < 2; ++mode) {
            Rig r(96000);
            for (int p = 0; p < 6; ++p) r.values[p] = extreme ? Delay::params[p].maximum : Delay::params[p].minimum;
            r.values[kMode] = static_cast<float>(mode);
            r.left(noise(30000, 13, 1e6f));
            r.right(noise(30000, 14, 1e6f));
            r.clock(pulses(30000, 400));
            const Result out = r.run(30000, 33);
            REQUIRE(finite_within(out.l, 1e7f) && finite_within(out.r, 1e7f));
        }
    }
    // No allocation in process, with a clock, ping-pong and tone active.
    Rig w;
    w.values[kMode] = 1.f;
    w.values[kTone] = 0.5f;
    w.left(noise(2048, 15));
    w.clock(pulses(2048, 500));
    w.run(2048, 128, {}, true);
    // Interface: IDs, stereo pairs, one-voice outputs.
    REQUIRE(std::strcmp(Delay::inputs[0].id, "left") == 0 && std::strcmp(Delay::inputs[1].id, "right") == 0);
    REQUIRE(std::strcmp(Delay::inputs[0].stereo_pair, "stereo") == 0 &&
            std::strcmp(Delay::outputs[1].stereo_pair, "stereo") == 0);
    REQUIRE(Delay::params.size() == 6 && std::strcmp(Delay::params[5].id, "mix") == 0);
    ws2_memory memory{sizeof(memory)};
    const ws2_config config{sizeof(config), 96000, 256, 16, 7, nullptr, 0};
    REQUIRE(w.d->memory_requirements(&config, &memory) == WS2_OK &&
            memory.large_bytes >= size_t(2) * 2 * 96000 * sizeof(float));
}

int main() {
    echo_position();
    feedback_decay();
    ping_pong();
    clock_sync();
    mix_law();
    unpatched_right();
    poly_sum();
    loop_safety();
    tone_shapes_loop();
    matrices();
    state_and_extremes();
    std::puts("fx.delay tests passed");
    return 0;
}
