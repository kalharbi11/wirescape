// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <cstdio>
#include <vector>

using spectral::Spectral;
namespace {
constexpr uint32_t kMaxFrames = 256, kMaxChannels = 16, kParams = spectral::kParams;
const double kTwoPi = 6.283185307179586;

struct Arena {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(4u << 20);
    size_t used = 0;
    static void *WS2_CALL alloc(void *context, size_t size, size_t alignment, uint32_t) {
        auto &a = *static_cast<Arena *>(context);
        const uintptr_t base = reinterpret_cast<uintptr_t>(a.bytes.data());
        const uintptr_t next = (base + a.used + alignment - 1) & ~(uintptr_t(alignment) - 1);
        if (next - base + size > a.bytes.size()) return nullptr;
        a.used = next - base + size;
        return a.bytes.data() + (next - base);
    }
};

struct Rig {
    Arena arena;
    ws2_host host{sizeof(host), &arena, Arena::alloc};
    const ws2_descriptor *d = ws::Module<Spectral>::descriptor();
    void *instance = nullptr;
    double rate;
    std::vector<float> out_rows = std::vector<float>(size_t(kMaxChannels) * kMaxFrames);
    float *output_rows[kMaxChannels]{};
    const float *input_rows[2][kMaxChannels]{};
    ws2_signal inputs[2]{};
    float values[kParams]{};
    const float *value_rows[kParams]{};
    ws2_signal params[kParams]{};
    ws2_signal_out outputs[1]{};
    ws2_process ctx{};
    float zero = 0;

    explicit Rig(double sample_rate = 48000) : rate(sample_rate) {
        const uint64_t seed = 7;
        const ws2_config config{sizeof(config), rate, kMaxFrames, kMaxChannels, seed, nullptr, 0};
        instance = d->create(&host);
        REQUIRE(instance && d->prepare(instance, &config, &host) == WS2_OK);
        for (uint32_t c = 0; c < kMaxChannels; ++c) output_rows[c] = out_rows.data() + size_t(c) * kMaxFrames;
        outputs[0] = {sizeof(ws2_signal_out), output_rows, 1, 0, kMaxChannels};
        for (uint32_t p = 0; p < kParams; ++p) {
            values[p] = Spectral::params[p].initial;
            value_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = 1;
        ctx.input_count = 2;
        ctx.output_count = 1;
        ctx.param_count = kParams;
        ctx.inputs = inputs;
        ctx.outputs = outputs;
        ctx.params = params;
    }
    ~Rig() { d->destroy(instance); }
    Rig &set(uint32_t p, float v) {
        values[p] = v;
        return *this;
    }

    // Render A/B (equal length) in blocks of `block`; returns the output samples.
    std::vector<float> run(const std::vector<float> &a, const std::vector<float> &b, uint32_t block = kMaxFrames,
                           bool *any_constant = nullptr) {
        std::vector<float> y;
        for (size_t pos = 0; pos < a.size(); pos += block) {
            const uint32_t n = uint32_t(std::min<size_t>(block, a.size() - pos));
            input_rows[0][0] = a.data() + pos;
            input_rows[1][0] = b.data() + pos;
            for (int i = 0; i < 2; ++i) inputs[i] = {sizeof(ws2_signal), input_rows[i], 1, WS2_CONNECTED};
            ctx.frames = n;
            int result;
            {
                allocation_watch::Scope watch;
                result = d->process(instance, &ctx);
                REQUIRE(watch.allocations() == 0);
            }
            REQUIRE(result == WS2_OK);
            REQUIRE(outputs[0].channels == 1);
            if (any_constant && (outputs[0].flags & WS2_CONSTANT)) *any_constant = true;
            for (uint32_t i = 0; i < n; ++i) {
                const float v = output_rows[0][(outputs[0].flags & WS2_CONSTANT) ? 0 : i];
                REQUIRE(std::isfinite(v));
                y.push_back(v);
            }
            ctx.frame_position += n;
        }
        return y;
    }
    // One block of constant inputs; returns whether the output was marked CONSTANT.
    bool run_silent() {
        const float a = 0, b = 0;
        const uint32_t n = 128;
        static float va, vb;
        va = a;
        vb = b;
        input_rows[0][0] = &va;
        input_rows[1][0] = &vb;
        for (int i = 0; i < 2; ++i) inputs[i] = {sizeof(ws2_signal), input_rows[i], 1, WS2_CONSTANT | WS2_CONNECTED};
        ctx.frames = n;
        REQUIRE(d->process(instance, &ctx) == WS2_OK);
        ctx.frame_position += n;
        return outputs[0].flags & WS2_CONSTANT;
    }
    uint32_t latency() const { return d->latency_frames(instance); }
    Bytes save() {
        Bytes bytes;
        ws2_writer w{sizeof(w), &bytes, sink};
        REQUIRE(d->save_state(instance, &w) == WS2_OK);
        return bytes;
    }
    int load(const Bytes &bytes) {
        Source s{&bytes, 0};
        ws2_reader r{sizeof(r), &s, source, uint32_t(bytes.size())};
        return d->load_state(instance, &r);
    }
};

struct Tone {
    double hz, amp;
};
struct NoiseSpec {
    uint32_t seed;
    float amp;
};
struct Span {
    size_t from, to;
};
std::vector<float> sine(size_t n, Tone tone, double rate) {
    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i) x[i] = float(tone.amp * std::sin(kTwoPi * tone.hz * double(i) / rate));
    return x;
}
std::vector<float> noise(size_t n, NoiseSpec spec) {
    uint32_t seed = spec.seed;
    const float amp = spec.amp;
    std::vector<float> x(n);
    for (auto &v : x) {
        seed = seed * 1664525u + 1013904223u;
        v = amp * (float(seed >> 8) / 8388608.f - 1.f);
    }
    return x;
}
std::vector<float> add(std::vector<float> a, const std::vector<float> &b, float gb = 1) {
    for (size_t i = 0; i < a.size(); ++i) a[i] += gb * b[i];
    return a;
}
// Relative RMS error of y[from..to) against reference[i - delay].
double rel_error(const std::vector<float> &y, const std::vector<float> &ref, size_t delay, Span span) {
    const size_t from = span.from, to = span.to;
    double e = 0, r = 0;
    for (size_t i = from; i < to; ++i) {
        const double d = double(y[i]) - ref[i - delay];
        e += d * d;
        r += double(ref[i - delay]) * ref[i - delay];
    }
    return std::sqrt(e / std::max(r, 1e-30));
}
double rms(const std::vector<float> &y, Span span) {
    const size_t from = span.from, to = span.to;
    double s = 0;
    for (size_t i = from; i < to; ++i) s += double(y[i]) * y[i];
    return std::sqrt(s / double(to - from));
}
// Hann-windowed magnitude spectrum by a direct DFT (independent of the module's FFT).
std::vector<double> magnitudes(const std::vector<float> &x, Span span) {
    const size_t from = span.from, n = span.to - span.from;
    std::vector<double> m(n / 2);
    for (size_t k = 0; k < n / 2; ++k) {
        double re = 0, im = 0;
        for (size_t j = 0; j < n; ++j) {
            const double w = .5 - .5 * std::cos(kTwoPi * double(j) / double(n)), v = x[from + j] * w;
            re += v * std::cos(kTwoPi * double(k * j) / double(n));
            im -= v * std::sin(kTwoPi * double(k * j) / double(n));
        }
        m[k] = std::hypot(re, im);
    }
    return m;
}
double spectral_distance(const std::vector<double> &a, const std::vector<double> &b) {
    double e = 0, r = 0;
    for (size_t k = 0; k < a.size(); ++k) {
        e += (a[k] - b[k]) * (a[k] - b[k]);
        r += b[k] * b[k];
    }
    return std::sqrt(e / r);
}
double max_step(const std::vector<float> &y, Span span) {
    const size_t from = span.from, to = span.to;
    double s = 0;
    for (size_t i = from + 1; i < to; ++i) s = std::max(s, double(std::abs(y[i] - y[i - 1])));
    return s;
}

void test_fft_matches_dft() {
    for (uint32_t n : {16u, 64u, 512u, 4096u}) {
        std::vector<float> storage(spectral::RealFft::floats(8192)), x = noise(n, NoiseSpec{n, .3f}), re(n / 2 + 1),
                                                                     im(n / 2 + 1), back(n);
        spectral::RealFft fft;
        fft.init(8192, storage.data());
        fft.forward(n, x.data(), {re.data(), im.data()});
        double worst = 0, scale = 0;
        for (uint32_t k = 0; k <= n / 2; k += (n > 512 ? 37 : 1)) {
            double dr = 0, di = 0;
            for (uint32_t j = 0; j < n; ++j) {
                dr += x[j] * std::cos(kTwoPi * k * j / n);
                di -= x[j] * std::sin(kTwoPi * k * j / n);
            }
            worst = std::max(worst, std::hypot(dr - re[k], di - im[k]));
            scale = std::max(scale, std::hypot(dr, di));
        }
        REQUIRE(worst <= 1e-4 * std::max(1.0, scale));
        fft.inverse(n, {re.data(), im.data()}, back.data());
        for (uint32_t j = 0; j < n; ++j) REQUIRE(std::abs(back[j] - x[j]) < 1e-5f);
    }
}

std::vector<float> test_a(size_t n, double rate) {
    return add(add(sine(n, Tone{220, .5}, rate), sine(n, Tone{1310, .2}, rate)), noise(n, NoiseSpec{3, .05f}));
}
std::vector<float> test_b(size_t n, double rate) {
    return add(sine(n, Tone{523.25, .4}, rate), noise(n, NoiseSpec{9, .1f}));
}

void test_frame_matrix_every_mode() {
    const size_t n = 6000;
    const auto a = test_a(n, 48000), b = test_b(n, 48000);
    for (uint32_t mode = 0; mode < spectral::kModes; ++mode) {
        Rig ref;
        ref.set(spectral::P_MODE, float(mode)).set(spectral::P_SIZE, 0);
        const auto expected = ref.run(a, b);
        REQUIRE(rms(expected, {2000, n}) > 1e-3);
        for (uint32_t block : {1u, 17u, 64u, 256u}) {
            Rig r;
            r.set(spectral::P_MODE, float(mode)).set(spectral::P_SIZE, 0);
            const auto got = r.run(a, b, block);
            for (size_t i = 0; i < n; ++i) REQUIRE(same_element(got[i], expected[i]));
        }
    }
}

// Max with B silent passes A through unchanged, delayed by exactly the latency, at every rate.
void test_rates_identity_and_latency() {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (uint32_t size = 0; size < 4; ++size) {
            Rig r(rate);
            r.set(spectral::P_MODE, spectral::Max).set(spectral::P_SIZE, float(size));
            const uint32_t latency = spectral::kBaseSize[size] * (rate > 66000 ? 2 : 1);
            const size_t n = 4 * latency + 2000;
            const auto a = test_a(n, rate), silent = std::vector<float>(n, 0.f);
            const auto y = r.run(a, silent);
            REQUIRE(r.latency() == latency);
            REQUIRE(rel_error(y, a, latency, {latency + latency, n}) < 1e-4);
            for (size_t i = 0; i < latency / 2; ++i)
                REQUIRE(std::abs(y[i]) < 1e-4f); // only FFT roundoff before the latency
        }
    }
}

// Inputs are SUM: the module renders one voice whatever the block's channel count.
void test_one_voice_output() {
    const size_t n = 3000;
    const auto a = test_a(n, 48000), b = test_b(n, 48000);
    Rig one;
    one.set(spectral::P_SIZE, 0);
    Rig four;
    four.set(spectral::P_SIZE, 0);
    four.ctx.channels = 4;
    four.outputs[0].channels = 4;
    const auto x = one.run(a, b), y = four.run(a, b);
    for (size_t i = 0; i < n; ++i) REQUIRE(same_element(x[i], y[i]));
}

void test_reset_determinism() {
    const size_t n = 5000;
    const auto a = test_a(n, 48000), b = test_b(n, 48000);
    Rig fresh;
    fresh.set(spectral::P_SIZE, 0);
    const auto expected = fresh.run(a, b);
    Rig used;
    used.set(spectral::P_SIZE, 0);
    used.run(b, a);
    used.d->reset(used.instance);
    const auto got = used.run(a, b);
    for (size_t i = 0; i < n; ++i) REQUIRE(same_element(got[i], expected[i]));
}

// State keeps the schedule (hop phase, modes, size, RNG) but not the audio history.
void test_state_round_trip() {
    const size_t warm = 3333, n = 6000; // warm is not a multiple of the hop
    const auto a = test_a(warm + n, 48000), b = test_b(warm + n, 48000);
    const std::vector<float> a0(a.begin(), a.begin() + warm), b0(b.begin(), b.begin() + warm);
    const std::vector<float> a1(a.begin() + warm, a.end()), b1(b.begin() + warm, b.end());
    Rig x;
    x.set(spectral::P_MODE, spectral::Max).set(spectral::P_SIZE, 0);
    x.run(a0, b0);
    const Bytes state = x.save();
    Rig y;
    y.set(spectral::P_MODE, spectral::Max).set(spectral::P_SIZE, 0);
    REQUIRE(y.load(state) == WS2_OK);
    Rig z;
    z.set(spectral::P_MODE, spectral::Max).set(spectral::P_SIZE, 0);
    z.run(b0, a0); // different history, same loaded schedule
    REQUIRE(z.load(state) == WS2_OK);
    const auto ox = x.run(a1, b1), oy = y.run(a1, b1), oz = z.run(a1, b1);
    for (size_t i = 0; i < n; ++i) REQUIRE(same_element(oy[i], oz[i]));
    // Once one window of new input has passed, the hop grid matches the uninterrupted run.
    for (size_t i = 1024; i < n; ++i) REQUIRE(same_element(ox[i], oy[i]));

    Bytes unknown = state;
    unknown[0] = 99;
    REQUIRE(y.load(unknown) == WS2_UNSUPPORTED);
    for (size_t cut = 0; cut < state.size(); cut += 4) {
        Bytes shorter;
        for (size_t i = 0; i < cut; ++i) shorter.push_back(state[i]);
        REQUIRE(y.load(shorter) == WS2_INVALID);
    }
    Bytes bad = state;
    bad[8] = 200; // mode out of range
    REQUIRE(y.load(bad) == WS2_INVALID);
    // A refused load leaves the current state alone: y and z still agree.
    const auto more = test_b(2000, 48000);
    const auto py = y.run(more, more), pz = z.run(more, more);
    for (size_t i = 0; i < py.size(); ++i) REQUIRE(same_element(py[i], pz[i]));
}

void test_constant() {
    Rig r;
    r.set(spectral::P_SIZE, 0);
    REQUIRE(r.run_silent()); // fresh and silent
    const auto a = test_a(2000, 48000), b = test_b(2000, 48000);
    bool constant = false;
    r.run(a, b, 128, &constant);
    REQUIRE(!constant);       // running audio is never constant
    REQUIRE(!r.run_silent()); // the tail still sounds
    bool settled = false;
    for (int i = 0; i < 20; ++i) settled = r.run_silent();
    REQUIRE(settled);
}

void test_extremes_stay_finite() {
    const size_t n = 9000;
    std::vector<float> hot(n), hot_b = noise(n, NoiseSpec{5, 10.f});
    for (size_t i = 0; i < n; ++i) hot[i] = (i / 37) % 2 ? 10.f : -10.f;
    for (uint32_t mode = 0; mode < spectral::kModes; ++mode)
        for (int corner = 0; corner < 2; ++corner) {
            Rig r;
            for (uint32_t p = 0; p < kParams; ++p)
                r.set(p, corner ? Spectral::params[p].maximum : Spectral::params[p].minimum);
            r.set(spectral::P_MODE, float(mode));
            r.run(hot, hot_b); // run() requires every sample finite
            r.run(hot_b, std::vector<float>(n, 0.f));
            r.run(std::vector<float>(n, 0.f), hot);
        }
}

void test_morph_ends_match_sources() {
    const size_t n = 20000, len = 4096, from = 12000;
    const auto a = add(sine(n, Tone{220, .5}, 48000), sine(n, Tone{1310, .25}, 48000)),
               b = add(sine(n, Tone{523.25, .4}, 48000), sine(n, Tone{3100, .1}, 48000));
    for (float m : {0.f, 1.f}) {
        Rig r;
        r.set(spectral::P_MODE, spectral::Morph).set(spectral::P_MORPH, m);
        const auto y = r.run(a, b);
        const uint32_t latency = r.latency();
        const auto got = magnitudes(y, {from, from + len}),
                   want = magnitudes(m == 0 ? a : b, {from - latency, from - latency + len});
        const double distance = spectral_distance(got, want);
        std::printf("morph %.0f spectral distance %.4f\n", m, distance);
        REQUIRE(distance < 0.1);
    }
    Rig mid;
    mid.set(spectral::P_MODE, spectral::Morph).set(spectral::P_MORPH, .5f).set(spectral::P_CURVE, 1);
    const auto y = mid.run(a, b); // linear middle carries both sources' partials
    const auto got = magnitudes(y, {from, from + len});
    const double hz = 48000.0 / len;
    REQUIRE(got[size_t(std::lround(220 / hz))] > .1 * magnitudes(a, {from, from + len})[size_t(std::lround(220 / hz))]);
    REQUIRE(got[size_t(std::lround(523.25 / hz))] >
            .1 * magnitudes(b, {from, from + len})[size_t(std::lround(523.25 / hz))]);
}

void test_cross_amount_zero_is_b() {
    const size_t n = 12000;
    const auto a = test_a(n, 48000), b = test_b(n, 48000);
    Rig r;
    r.set(spectral::P_MODE, spectral::Cross).set(spectral::P_AMOUNT, 0);
    const auto y = r.run(a, b);
    REQUIRE(rel_error(y, b, r.latency(), {size_t(2) * r.latency(), n}) < 1e-4);
    Rig full;
    full.set(spectral::P_MODE, spectral::Cross).set(spectral::P_AMOUNT, 1).set(spectral::P_SMOOTH, 1);
    const auto z = full.run(a, b); // A's envelope on B: louder than B where A is loud
    REQUIRE(rms(z, {6000, n}) > .01);
}

// Two well separated sines: per-bin max keeps both, mean halves each, min keeps neither, A - B keeps A.
void test_combine_math() {
    const size_t n = 16000, from = 8000;
    const auto a = sine(n, Tone{300, .5}, 48000), b = sine(n, Tone{3000, .5}, 48000), both = add(a, b);
    struct Case {
        uint32_t mode;
        std::vector<float> expect;
        double tolerance;
    };
    std::vector<float> half = both;
    for (auto &v : half) v *= .5f;
    for (const Case &c :
         {Case{spectral::Max, both, .02}, Case{spectral::Mean, half, .02}, Case{spectral::Diff, a, .02}}) {
        Rig r;
        r.set(spectral::P_MODE, float(c.mode));
        const auto y = r.run(a, b);
        const double e = rel_error(y, c.expect, r.latency(), {from, n});
        std::printf("combine mode %u error %.4f\n", c.mode, e);
        REQUIRE(e < c.tolerance);
    }
    Rig mn;
    mn.set(spectral::P_MODE, spectral::Min);
    REQUIRE(rms(mn.run(a, b), {from, n}) < .02 * rms(a, {from, n}));
    // Identical inputs: max = mean = min = A, A - B = silence.
    for (uint32_t mode : {spectral::Max, spectral::Mean, spectral::Min}) {
        Rig r;
        r.set(spectral::P_MODE, float(mode));
        REQUIRE(rel_error(r.run(a, a), a, r.latency(), {from, n}) < 1e-4);
    }
    Rig d;
    d.set(spectral::P_MODE, spectral::Diff);
    REQUIRE(rms(d.run(a, a), {from, n}) < 1e-5);
    Rig w;
    w.set(spectral::P_MODE, spectral::Max).set(spectral::P_BLEVEL, 0);
    REQUIRE(rel_error(w.run(a, b), a, w.latency(), {from, n}) < 1e-4); // B weight 0 removes B
}

// A step on the mode CV crossfades over four frames: no output step beyond the steady-state ones.
void test_mode_switch_crossfades() {
    const size_t n = 24000, half = 12000;
    const auto a = sine(n, Tone{330, .5}, 48000), b = sine(n, Tone{550, .5}, 48000);
    double steady = 0;
    for (uint32_t mode : {spectral::Max, spectral::Diff, spectral::Morph, spectral::Cross}) {
        Rig r;
        r.set(spectral::P_MODE, float(mode));
        steady = std::max(steady, max_step(r.run(a, b), {6000, n}));
    }
    for (auto [from, to] : {std::pair{spectral::Max, spectral::Diff}, std::pair{spectral::Diff, spectral::Morph},
                            std::pair{spectral::Morph, spectral::Cross}}) {
        Rig r;
        r.set(spectral::P_MODE, float(from));
        const std::vector<float> a0(a.begin(), a.begin() + half), b0(b.begin(), b.begin() + half);
        const std::vector<float> a1(a.begin() + half, a.end()), b1(b.begin() + half, b.end());
        auto y = r.run(a0, b0);
        r.set(spectral::P_MODE, float(to) + .3f); // CV lands between steps: rounds to `to`
        const auto y1 = r.run(a1, b1);
        y.insert(y.end(), y1.begin(), y1.end());
        const double step = max_step(y, {6000, n});
        std::printf("mode %u -> %u: worst step %.4f (steady %.4f)\n", from, to, step, steady);
        REQUIRE(step <= 1.25 * steady);
    }
}

// Changing the resolution drains the old frames, then the latency follows the new size.
void test_size_change() {
    const size_t n = 30000, half = 10000;
    const auto a = sine(n, Tone{330, .5}, 48000), b = sine(n, Tone{550, .5}, 48000);
    Rig r;
    r.set(spectral::P_MODE, spectral::Max).set(spectral::P_SIZE, 3);
    REQUIRE(r.latency() == 2048); // plan-build value: the default size
    const std::vector<float> a0(a.begin(), a.begin() + half), b0(b.begin(), b.begin() + half);
    const std::vector<float> a1(a.begin() + half, a.end()), b1(b.begin() + half, b.end());
    auto y = r.run(a0, b0);
    REQUIRE(r.latency() == 4096);
    r.set(spectral::P_SIZE, 0);
    const auto y1 = r.run(a1, b1);
    REQUIRE(r.latency() == 512);
    y.insert(y.end(), y1.begin(), y1.end());
    double steady = max_step(y, {8000, half});
    REQUIRE(max_step(y, {8000, n}) <= 1.25 * steady);
    REQUIRE(rel_error(y, add(a, b), 512, {half + 8000, n}) < .02);
}
} // namespace

int main() {
    test_fft_matches_dft();
    test_frame_matrix_every_mode();
    test_rates_identity_and_latency();
    test_one_voice_output();
    test_reset_determinism();
    test_state_round_trip();
    test_constant();
    test_extremes_stay_finite();
    test_morph_ends_match_sources();
    test_cross_amount_zero_is_b();
    test_combine_math();
    test_mode_switch_crossfades();
    test_size_change();
    std::puts("abi2_spectral_tests passed");
    return 0;
}
