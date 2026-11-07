// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <cstdio>
#include <vector>

using blend::Blend;
namespace {
constexpr uint32_t kMaxFrames = 256, kMaxChannels = 16, kParams = blend::kParams;
const double kTwoPi = 6.283185307179586;
using Signal = std::vector<float>;

struct Rig {
    Pool pool;
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor *d = ws::Module<Blend>::descriptor();
    void *instance = nullptr;
    std::vector<float> out_rows = std::vector<float>(size_t(kMaxChannels) * kMaxFrames);
    float *output_rows[kMaxChannels]{};
    const float *input_rows[3][kMaxChannels]{};
    ws2_signal inputs[3]{};
    float values[kParams]{};
    const float *value_rows[kParams]{};
    ws2_signal params[kParams]{};
    ws2_signal_out outputs[1]{};
    ws2_process ctx{};

    explicit Rig(double rate = 48000) {
        const ws2_config config{sizeof(config), rate, kMaxFrames, kMaxChannels, 7, nullptr, 0};
        instance = d->create(&host);
        REQUIRE(instance && d->prepare(instance, &config, &host) == WS2_OK);
        for (uint32_t c = 0; c < kMaxChannels; ++c) output_rows[c] = out_rows.data() + size_t(c) * kMaxFrames;
        outputs[0] = {sizeof(ws2_signal_out), output_rows, 1, 0, kMaxChannels};
        for (uint32_t p = 0; p < kParams; ++p) {
            values[p] = Blend::params[p].initial;
            value_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = 1;
        ctx.input_count = 3;
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
    // Inputs as varying rows; `constant` marks every block's inputs CONSTANT (they must be flat).
    Signal run(const Signal &a, const Signal &b, const Signal &sw, uint32_t block = kMaxFrames, bool constant = false,
               bool *any_constant = nullptr) {
        Signal y;
        for (size_t pos = 0; pos < a.size(); pos += block) {
            const uint32_t n = uint32_t(std::min<size_t>(block, a.size() - pos));
            const Signal *rows[3] = {&a, &b, &sw};
            for (int i = 0; i < 3; ++i) {
                input_rows[i][0] = rows[i]->data() + pos;
                inputs[i] = {sizeof(ws2_signal), input_rows[i], 1, WS2_CONNECTED | (constant ? WS2_CONSTANT : 0u)};
            }
            ctx.frames = n;
            int result;
            {
                allocation_watch::Scope watch;
                result = d->process(instance, &ctx);
                REQUIRE(watch.allocations() == 0);
            }
            REQUIRE(result == WS2_OK && outputs[0].channels == 1);
            const bool flat = outputs[0].flags & WS2_CONSTANT;
            if (any_constant && flat) *any_constant = true;
            for (uint32_t i = 0; i < n; ++i) {
                const float v = output_rows[0][flat ? 0 : i];
                REQUIRE(std::isfinite(v));
                y.push_back(v);
            }
            ctx.frame_position += n;
        }
        return y;
    }
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
Signal sine(size_t n, Tone t, double rate) {
    Signal x(n);
    for (size_t i = 0; i < n; ++i) x[i] = float(t.amp * std::sin(kTwoPi * t.hz * double(i) / rate));
    return x;
}
Signal flat(size_t n, float v) { return Signal(n, v); }
struct Pulses {
    size_t every, width;
};
Signal pulses(size_t n, Pulses p) {
    const size_t every = p.every, width = p.width;
    Signal x(n, 0.f);
    for (size_t i = every; i < n; i += every)
        for (size_t j = 0; j < width && i + j < n; ++j) x[i + j] = 1.f;
    return x;
}
// Where the output of A = +1, B = -1 crosses zero (the position passes the middle), interpolated.
std::vector<double> crossings(const Signal &y) {
    std::vector<double> at;
    for (size_t i = 1; i < y.size(); ++i)
        if ((y[i - 1] > 0) != (y[i] > 0)) at.push_back(double(i - 1) + double(y[i - 1]) / double(y[i - 1] - y[i]));
    return at;
}
double max_step(const Signal &y, size_t from) {
    double s = 0;
    for (size_t i = from + 1; i < y.size(); ++i) s = std::max(s, double(std::abs(y[i] - y[i - 1])));
    return s;
}

void test_frame_matrix() {
    const size_t n = 20000;
    const Signal a = sine(n, Tone{220, .5}, 48000), b = sine(n, Tone{331, .4}, 48000), sw = pulses(n, Pulses{3001, 50});
    for (float mode : {0.f, 1.f}) {
        Rig ref;
        ref.set(blend::P_MODE, mode).set(blend::P_BALANCE, .3f).set(blend::P_SEG_A, .03f).set(blend::P_SEG_B, .05f);
        const Signal expected = ref.run(a, b, sw);
        for (uint32_t block : {1u, 17u, 64u, 256u}) {
            Rig r;
            r.set(blend::P_MODE, mode).set(blend::P_BALANCE, .3f).set(blend::P_SEG_A, .03f).set(blend::P_SEG_B, .05f);
            const Signal got = r.run(a, b, sw, block);
            for (size_t i = 0; i < n; ++i) REQUIRE(same_element(got[i], expected[i]));
        }
    }
}

// Splice without jitter alternates segments of exactly seg_a and seg_b at every rate.
void test_segments_exact_at_every_rate() {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        const size_t n = size_t(rate * 1.2);
        Rig r(rate);
        r.set(blend::P_MODE, blend::Splice).set(blend::P_JITTER, 0).set(blend::P_SEG_A, .05f).set(blend::P_SEG_B, .08f);
        const auto at = crossings(r.run(flat(n, 1), flat(n, -1), flat(n, 0)));
        REQUIRE(at.size() > 8);
        const double sa = std::round(.05 * rate), sb = std::round(.08 * rate);
        for (size_t k = 1; k < at.size(); ++k) { // splice starts on B (position 0.5 rounds up), then A, B, ...
            const double want = (k % 2) ? sa : sb;
            REQUIRE(std::abs(at[k] - at[k - 1] - want) < 1e-3);
        }
    }
}

// Equal power: balance 0 / 1 are A / B, the middle is -3 dB per side.
void test_equal_power_balance() {
    const size_t n = 2000;
    for (auto [balance, ga, gb] :
         {std::tuple{0.f, 1.0, 0.0}, std::tuple{1.f, 0.0, 1.0}, std::tuple{.5f, std::sqrt(.5), std::sqrt(.5)}}) {
        Rig ra, rb;
        ra.set(blend::P_BALANCE, balance);
        rb.set(blend::P_BALANCE, balance);
        const Signal ya = ra.run(flat(n, 1), flat(n, 0), flat(n, 0)), yb = rb.run(flat(n, 0), flat(n, 1), flat(n, 0));
        REQUIRE(std::abs(ya[n - 1] - ga) < 1e-6 && std::abs(yb[n - 1] - gb) < 1e-6);
    }
}

// Each switch is a raised cosine of exactly `fade` samples, and power stays constant through it.
void test_fade_shape_and_power() {
    const size_t n = 12000;
    const float fade = .005f;
    const uint32_t len = uint32_t(std::round(.005 * 48000));
    Rig ra, rb;
    for (Rig *r : {&ra, &rb})
        r->set(blend::P_MODE, blend::Splice).set(blend::P_JITTER, 0).set(blend::P_FADE, fade).set(blend::P_SEG_A, .1f);
    for (Rig *r : {&ra, &rb}) r->set(blend::P_SEG_B, .1f);
    const Signal ga = ra.run(flat(n, 1), flat(n, 0), flat(n, 0)), gb = rb.run(flat(n, 0), flat(n, 1), flat(n, 0));
    for (size_t i = 0; i < n; ++i) REQUIRE(std::abs(ga[i] * ga[i] + gb[i] * gb[i] - 1) < 1e-5);
    const size_t start = 4800; // first switch to A: the B segment from sample 0 is 0.1 s long
    REQUIRE(std::abs(gb[start - 1] - 1) < 1e-6);
    for (uint32_t k = 1; k <= len; ++k) {
        const double p = 1 - (.5 - .5 * std::cos(3.141592653589793 * k / len)); // B -> A
        REQUIRE(std::abs(gb[start - 1 + k] - std::sin(1.5707963267948966 * p)) < 1e-5);
    }
    REQUIRE(std::abs(ga[start + len] - 1) < 1e-6);
}

// Jitter varies each length within +-jitter around the setting.
void test_jitter_bounds() {
    const size_t n = size_t(48000) * 12;
    Rig r;
    r.set(blend::P_MODE, blend::Splice).set(blend::P_JITTER, .5f).set(blend::P_SEG_A, .05f).set(blend::P_SEG_B, .05f);
    const auto at = crossings(r.run(flat(n, 1), flat(n, -1), flat(n, 0)));
    double sum = 0, lo = 1e9, hi = 0;
    for (size_t k = 1; k < at.size(); ++k) {
        const double l = at[k] - at[k - 1];
        sum += l;
        lo = std::min(lo, l);
        hi = std::max(hi, l);
    }
    const double mean = sum / double(at.size() - 1), want = .05 * 48000;
    std::printf("jitter 0.5: %zu segments, mean %.0f (want %.0f), range %.0f..%.0f\n", at.size() - 1, mean, want, lo,
                hi);
    // Uniform +-50%: each length has sd 0.5 / sqrt(3) of the setting; allow 4 sd of the mean.
    const double bound = 4 * .5 / std::sqrt(3.0) / std::sqrt(double(at.size() - 1));
    REQUIRE(at.size() > 200 && std::abs(mean / want - 1) < bound);
    REQUIRE(lo >= .5 * want - 1 && hi <= 1.5 * want + 1 && hi - lo > .5 * want);
}

// A rising edge on `switch` starts the change at its own sample, in both modes.
void test_switch_trigger() {
    const size_t n = 6000, edge = 1000;
    Signal sw = flat(n, 0);
    for (size_t i = edge; i < edge + 20; ++i) sw[i] = 1;
    Rig x;
    x.set(blend::P_BALANCE, 0); // all A until the trigger flips the side balance measures from
    const Signal y = x.run(flat(n, 1), flat(n, -1), sw);
    REQUIRE(y[edge - 1] == 1.f && y[edge] < 1.f && std::abs(y[n - 1] + 1) < 1e-6);
    Rig s;
    s.set(blend::P_MODE, blend::Splice).set(blend::P_JITTER, 0).set(blend::P_SEG_A, 2).set(blend::P_SEG_B, 2);
    const Signal z = s.run(flat(n, 1), flat(n, -1), sw);
    REQUIRE(std::abs(z[edge - 1] + 1) < 1e-6 && z[edge] > z[edge - 1] && std::abs(z[n - 1] - 1) < 1e-6);
}

// Changing mode glides: no output step beyond the steady ones.
void test_mode_change_glides() {
    const size_t n = 24000, half = 12000;
    const Signal a = sine(n, Tone{220, .5}, 48000), b = sine(n, Tone{330, .5}, 48000), sw = flat(n, 0);
    Rig steady;
    steady.set(blend::P_MODE, blend::Splice);
    const double bound = std::max(max_step(steady.run(a, b, sw), 2000), max_step(Rig().run(a, b, sw), 2000));
    for (float from : {0.f, 1.f}) {
        Rig r;
        r.set(blend::P_MODE, from).set(blend::P_SEG_A, 2).set(blend::P_SEG_B, 2);
        const Signal a0(a.begin(), a.begin() + half), b0(b.begin(), b.begin() + half),
            s0(sw.begin(), sw.begin() + half);
        const Signal a1(a.begin() + half, a.end()), b1(b.begin() + half, b.end()), s1(sw.begin() + half, sw.end());
        Signal y = r.run(a0, b0, s0);
        r.set(blend::P_MODE, 1 - from);
        const Signal y1 = r.run(a1, b1, s1);
        y.insert(y.end(), y1.begin(), y1.end());
        REQUIRE(max_step(y, 2000) <= 1.25 * bound);
    }
}

void test_reset_and_state() {
    const size_t n = 15000, cut = 7777;
    const Signal a = sine(n, Tone{220, .5}, 48000), b = sine(n, Tone{331, .4}, 48000), sw = pulses(n, Pulses{2500, 30});
    auto configure = [](Rig &r) {
        r.set(blend::P_MODE, blend::Splice)
            .set(blend::P_SEG_A, .02f)
            .set(blend::P_SEG_B, .03f)
            .set(blend::P_FADE, .02f);
    };
    Rig fresh;
    configure(fresh);
    const Signal expected = fresh.run(a, b, sw);
    Rig used;
    configure(used);
    used.run(b, a, sw);
    used.d->reset(used.instance);
    const Signal again = used.run(a, b, sw);
    for (size_t i = 0; i < n; ++i) REQUIRE(same_element(again[i], expected[i]));

    Rig x;
    configure(x);
    x.run(Signal(a.begin(), a.begin() + cut), Signal(b.begin(), b.begin() + cut), Signal(sw.begin(), sw.begin() + cut));
    const Bytes state = x.save();
    Rig y;
    configure(y);
    REQUIRE(y.load(state) == WS2_OK);
    const Signal a1(a.begin() + cut, a.end()), b1(b.begin() + cut, b.end()), s1(sw.begin() + cut, sw.end());
    const Signal ox = x.run(a1, b1, s1), oy = y.run(a1, b1, s1);
    for (size_t i = 0; i < ox.size(); ++i) REQUIRE(same_element(ox[i], oy[i])); // full state: exact continuation
    for (size_t i = 0; i < ox.size(); ++i) REQUIRE(same_element(ox[i], expected[cut + i]));

    Bytes unknown = state;
    unknown[0] = 9;
    REQUIRE(y.load(unknown) == WS2_UNSUPPORTED);
    for (size_t cutoff = 0; cutoff < state.size(); cutoff += 4) {
        Bytes shorter;
        for (size_t i = 0; i < cutoff; ++i) shorter.push_back(state[i]);
        REQUIRE(y.load(shorter) == WS2_INVALID);
    }
    Bytes bad = state;
    bad[20] = 7; // mode out of range
    REQUIRE(y.load(bad) == WS2_INVALID);
}

void test_constant_and_extremes() {
    Rig r;
    bool constant = false;
    r.run(flat(512, .3f), flat(512, -.2f), flat(512, 0), 128, true, &constant);
    REQUIRE(constant); // Crossfade with constant inputs and no glide
    Rig s;
    s.set(blend::P_MODE, blend::Splice);
    constant = false;
    s.run(flat(4096, .3f), flat(4096, -.2f), flat(4096, 0), 128, true, &constant);
    REQUIRE(!constant); // the splice clock always moves
    const size_t n = 48000;
    Signal hot(n), hot_b(n);
    for (size_t i = 0; i < n; ++i) {
        hot[i] = (i / 7) % 2 ? 10.f : -10.f;
        hot_b[i] = (i / 3) % 2 ? -10.f : 10.f;
    }
    for (int corner = 0; corner < 2; ++corner)
        for (float mode : {0.f, 1.f}) {
            Rig e;
            for (uint32_t p = 0; p < kParams; ++p)
                e.set(p, corner ? Blend::params[p].maximum : Blend::params[p].minimum);
            e.set(blend::P_MODE, mode);
            const Signal y = e.run(hot, hot_b, pulses(n, Pulses{97, 3})); // run() checks every sample is finite
            for (float v : y) REQUIRE(std::abs(v) <= 10 * 1.4143f);
        }
}
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    test_frame_matrix();
    test_segments_exact_at_every_rate();
    test_equal_power_balance();
    test_fade_shape_and_power();
    test_jitter_bounds();
    test_switch_trigger();
    test_mode_change_glides();
    test_reset_and_state();
    test_constant_and_extremes();
    std::puts("abi2_blend_tests passed");
    return 0;
}
