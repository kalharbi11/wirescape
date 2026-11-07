// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <numeric>
#include <vector>

using reverb::Reverb;
namespace {
constexpr uint32_t kFrames = 256, kMaxChannels = 16;
using Samples = std::vector<float>;
struct Stereo {
    Samples l, r;
};
struct Arena {
    std::unique_ptr<unsigned char[]> bytes = std::make_unique<unsigned char[]>(size_t(2) * 1024 * 1024);
    size_t used = 0;
};
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- the host allocation ABI fixes this signature.
void *WS2_CALL heap_alloc(void *ptr, size_t size, size_t align, uint32_t) {
    auto &arena = *static_cast<Arena *>(ptr);
    const auto start = reinterpret_cast<uintptr_t>(arena.bytes.get());
    const size_t offset = ((start + arena.used + align - 1) & ~(uintptr_t(align) - 1)) - start;
    if (offset + size > size_t(2) * 1024 * 1024) return nullptr;
    arena.used = offset + size;
    return arena.bytes.get() + offset;
}
struct Rig {
    Arena arena;
    ws2_host host{sizeof(host), &arena, heap_alloc};
    const ws2_descriptor *d = ws::Module<Reverb>::descriptor();
    void *instance = nullptr;
    float rows[2][kMaxChannels][kFrames]{};
    float *out_rows[2][kMaxChannels]{};
    const float *in_rows[2][1]{};
    ws2_signal inputs[2]{};
    float values[reverb::COUNT]{};
    const float *param_rows[reverb::COUNT]{};
    ws2_signal params[reverb::COUNT]{};
    ws2_signal_out outputs[2]{};
    ws2_process ctx{};
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- sample rate and voice count are distinct.
    explicit Rig(double rate = 48000, uint32_t channels = 1) {
        const ws2_config config{sizeof(config), rate, kFrames, kMaxChannels, 42, nullptr, 0};
        ws2_memory memory{sizeof(memory), 0, 0, 0};
        REQUIRE(d->memory_requirements(&config, &memory) == WS2_OK && memory.large_bytes > 100000);
        instance = d->create(&host);
        REQUIRE(instance && d->prepare(instance, &config, &host) == WS2_OK);
        for (uint32_t p = 0; p < 2; ++p) {
            for (uint32_t c = 0; c < kMaxChannels; ++c) out_rows[p][c] = rows[p][c];
            outputs[p] = {sizeof(ws2_signal_out), out_rows[p], 1, 0, kMaxChannels};
        }
        for (uint32_t p = 0; p < reverb::COUNT; ++p) {
            values[p] = Reverb::params[p].initial;
            param_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &param_rows[p], 1, WS2_CONSTANT};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = channels;
        ctx.input_count = 2;
        ctx.output_count = 2;
        ctx.param_count = reverb::COUNT;
        ctx.inputs = inputs;
        ctx.outputs = outputs;
        ctx.params = params;
    }
    ~Rig() { d->destroy(instance); }
    Rig &set(uint32_t p, float v) {
        values[p] = v;
        return *this;
    }
    Stereo run(const Samples &l, const Samples &r, uint32_t block = kFrames, bool right_connected = true) {
        REQUIRE(l.size() == r.size() && block > 0 && block <= kFrames);
        Stereo result;
        result.l.reserve(l.size());
        result.r.reserve(r.size());
        for (size_t pos = 0; pos < l.size(); pos += block) {
            const uint32_t n = uint32_t(std::min<size_t>(block, l.size() - pos));
            in_rows[0][0] = l.data() + pos;
            in_rows[1][0] = r.data() + pos;
            inputs[0] = {sizeof(ws2_signal), in_rows[0], 1, WS2_CONNECTED};
            inputs[1] = {sizeof(ws2_signal), in_rows[1], 1, right_connected ? WS2_CONNECTED : 0u};
            ctx.frames = n;
            int status = 0;
            {
                allocation_watch::Scope watch;
                status = d->process(instance, &ctx);
                REQUIRE(watch.allocations() == 0);
            }
            REQUIRE(status == WS2_OK && outputs[0].channels == 1 && outputs[1].channels == 1);
            REQUIRE(!(outputs[0].flags & WS2_CONSTANT) && !(outputs[1].flags & WS2_CONSTANT));
            for (uint32_t i = 0; i < n; ++i) {
                const float a = rows[0][0][i], b = rows[1][0][i];
                REQUIRE(std::isfinite(a) && std::isfinite(b));
                result.l.push_back(a);
                result.r.push_back(b);
            }
            ctx.frame_position += n;
        }
        return result;
    }
    Bytes save() {
        Bytes b;
        ws2_writer w{sizeof(w), &b, sink};
        REQUIRE(d->save_state(instance, &w) == WS2_OK);
        return b;
    }
    int load(const Bytes &bytes) {
        Source s{&bytes, 0};
        ws2_reader r{sizeof(r), &s, source, uint32_t(bytes.size())};
        return d->load_state(instance, &r);
    }
};
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- sample count and amplitude are distinct.
Samples noise(size_t n, float amplitude = .3f) {
    Samples x(n);
    uint32_t state = 0x52cau;
    for (auto &sample : x) {
        state = state * 1664525u + 1013904223u;
        sample = amplitude * (float(state >> 8) / float(0x00ffffffu) * 2.f - 1.f);
    }
    return x;
}
Samples silence(size_t n) { return Samples(n, 0); }
double rms(const Samples &s, size_t start, size_t end) {
    double energy = 0;
    for (size_t i = start; i < end; ++i) energy += double(s[i]) * s[i];
    return std::sqrt(energy / double(end - start));
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- symmetric source channels are intentional.
double correlation(const Samples &a, const Samples &b, size_t start) {
    double aa = 0, bb = 0, ab = 0, ma = 0, mb = 0;
    const double n = double(a.size() - start);
    for (size_t i = start; i < a.size(); ++i) {
        ma += a[i];
        mb += b[i];
    }
    ma /= n;
    mb /= n;
    for (size_t i = start; i < a.size(); ++i) {
        const double x = a[i] - ma, y = b[i] - mb;
        aa += x * x;
        bb += y * y;
        ab += x * y;
    }
    return ab / std::sqrt(aa * bb);
}
void test_lengths_and_rates() {
    for (double rate : {44100., 48000., 96000.}) {
        Reverb dsp;
        dsp.rate = float(rate);
        for (float size : {0.f, .2f, .5f, 1.f}) {
            for (uint32_t i = 0; i < 8; ++i) {
                const uint32_t a = dsp.scaled_length(i, size);
                REQUIRE(Reverb::prime(a) && a < Reverb::line_count(rate) - 2);
                for (uint32_t j = 0; j < i; ++j) REQUIRE(a != dsp.scaled_length(j, size));
            }
        }
        Rig rig(rate, 16);
        rig.set(reverb::MIX, 1).set(reverb::DAMPING, 0).set(reverb::MOTION, .4f);
        const auto y = rig.run(noise(size_t(rate * .15)), silence(size_t(rate * .15)), 17, false);
        REQUIRE(rms(y.l, 2000, y.l.size()) > 0);
    }
}
void test_frame_and_reset() {
    const Samples x = noise(12037), y = noise(12037, .2f);
    Rig one;
    one.set(reverb::MIX, 1).set(reverb::MOTION, .5f);
    const Stereo reference = one.run(x, y);
    for (uint32_t frames : {1u, 17u, 64u, 256u}) {
        Rig other;
        other.set(reverb::MIX, 1).set(reverb::MOTION, .5f);
        const Stereo got = other.run(x, y, frames);
        REQUIRE(got.l == reference.l && got.r == reference.r);
    }
    one.d->reset(one.instance);
    const Stereo fresh = one.run(x, y, 17);
    REQUIRE(fresh.l == reference.l && fresh.r == reference.r);
}
void test_dry_and_width() {
    const Samples x = noise(48000), y = noise(48000, .5f);
    Rig dry;
    dry.set(reverb::MIX, 0);
    const Stereo exact = dry.run(x, y, 17);
    REQUIRE(exact.l == x && exact.r == y);
    Rig normal;
    normal.set(reverb::MIX, 0);
    const Stereo mirrored = normal.run(x, y, 64, false);
    REQUIRE(mirrored.l == x && mirrored.r == x);
    Rig mono;
    mono.set(reverb::MIX, 1).set(reverb::WIDTH, 0).set(reverb::DAMPING, 0).set(reverb::MOTION, 0);
    const Stereo a = mono.run(x, x);
    REQUIRE(a.l == a.r && rms(a.l, 12000, a.l.size()) > 0);
    Rig wide;
    wide.set(reverb::MIX, 1).set(reverb::WIDTH, 1).set(reverb::DAMPING, 0).set(reverb::MOTION, 0);
    const Stereo b = wide.run(x, x);
    const double corr = correlation(b.l, b.r, 12000);
    std::printf("stereo correlation: %.4f\n", corr);
    REQUIRE(corr < .5);
}
double measured_rt60(float size) {
    constexpr uint32_t rate = 48000, window = 2400;
    Samples impulse(size_t(rate) * 3, 0);
    impulse[0] = 1;
    Rig rig;
    rig.set(reverb::SIZE, size)
        .set(reverb::DECAY, 2)
        .set(reverb::DAMPING, 0)
        .set(reverb::MOTION, 0)
        .set(reverb::PREDELAY, 0)
        .set(reverb::MIX, 1);
    const Stereo result = rig.run(impulse, impulse);
    // Least-squares fit to independent RMS-envelope measurements from 0.2 to 2.6 s.
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const int begin = 4, end = 52;
    for (int k = begin; k < end; ++k) {
        const double t = (k + .5) * double(window) / rate;
        const double energy = .5 * (std::pow(rms(result.l, size_t(k) * window, size_t(k + 1) * window), 2) +
                                    std::pow(rms(result.r, size_t(k) * window, size_t(k + 1) * window), 2));
        const double ln = std::log(std::sqrt(energy) + 1e-30);
        sx += t;
        sy += ln;
        sxx += t * t;
        sxy += t * ln;
    }
    const double slope = (double(end - begin) * sxy - sx * sy) / (double(end - begin) * sxx - sx * sx);
    return -std::log(1000.0) / slope;
}
void test_decay() {
    const double small = measured_rt60(.2f), large = measured_rt60(1.f);
    std::printf("RT60 size .2: %.3fs; size 1: %.3fs\n", small, large);
    REQUIRE(small > 1.6 && small < 2.4);
    REQUIRE(large > 1.6 && large < 2.4);
    REQUIRE(std::abs(small - large) / std::max(small, large) < .2);
}
void test_freeze() {
    constexpr size_t rate = 48000;
    const Samples first = noise(rate, .25f), second = noise(rate * 2, .25f);
    Rig r;
    r.set(reverb::MIX, 1).set(reverb::DAMPING, 0).set(reverb::MOTION, 0).set(reverb::DECAY, 5);
    const Stereo before = r.run(first, first);
    const double original = rms(before.l, rate * 3 / 4, rate);
    r.set(reverb::FREEZE, 1);
    const Stereo frozen = r.run(second, second);
    const double held = rms(frozen.l, rate / 5, rate * 2);
    const double db = 20 * std::log10(held / original);
    std::printf("freeze RMS change: %.3f dB\n", db);
    REQUIRE(std::abs(db) < 1.5);
    // Once the 20ms transition has finished, two distinct new inputs cannot enter.
    Rig a, b;
    for (Rig *rig : {&a, &b}) {
        rig->set(reverb::MIX, 1).set(reverb::DAMPING, 0).set(reverb::MOTION, 0);
        rig->run(first, first);
        rig->set(reverb::FREEZE, 1);
        rig->run(silence(rate / 5), silence(rate / 5));
    }
    const Stereo ax = a.run(noise(rate / 2, 1), noise(rate / 2, 1));
    const Stereo bx = b.run(silence(rate / 2), silence(rate / 2));
    REQUIRE(ax.l == bx.l && ax.r == bx.r);
}
void test_state_extremes_and_sweep() {
    const Samples x = noise(12000);
    Rig used;
    used.set(reverb::MIX, 1);
    used.run(x, x);
    const Bytes state = used.save();
    REQUIRE(state.size() == 4);
    Rig reset;
    reset.set(reverb::MIX, 1);
    REQUIRE(reset.load(state) == WS2_OK);
    const Stereo fresh = reset.run(x, x);
    used.d->reset(used.instance);
    const Stereo again = used.run(x, x);
    REQUIRE(fresh.l == again.l && fresh.r == again.r);
    Bytes bad = state;
    bad[0] = 99;
    REQUIRE(reset.load(bad) == WS2_UNSUPPORTED);
    bad.clear();
    REQUIRE(reset.load(bad) == WS2_INVALID);
    for (bool maximum : {false, true}) {
        Rig r(96000, 16);
        for (uint32_t p = 0; p < reverb::COUNT; ++p)
            r.values[p] = maximum ? Reverb::params[p].maximum : Reverb::params[p].minimum;
        const Stereo hot = r.run(noise(96000, 1), noise(96000, 1), 1);
        for (float v : hot.l) REQUIRE(std::abs(v) <= 2);
        for (float v : hot.r) REQUIRE(std::abs(v) <= 2);
    }
    Rig sweep;
    sweep.set(reverb::MIX, 1).set(reverb::DECAY, 20).set(reverb::DAMPING, 0);
    for (float size : {0.f, .25f, .5f, .75f, 1.f, .75f, .5f, .25f, 0.f}) {
        sweep.set(reverb::SIZE, size);
        const auto out = sweep.run(noise(12000, .25f), noise(12000, .25f));
        for (size_t i = 1; i < out.l.size(); ++i) REQUIRE(std::abs(out.l[i] - out.l[i - 1]) < .5f);
    }
    Rig stable;
    stable.set(reverb::DECAY, 20).set(reverb::DAMPING, 0).set(reverb::MOTION, 0).set(reverb::MIX, 1);
    const auto out = stable.run(noise(size_t(48000) * 30, 1), noise(size_t(48000) * 30, 1));
    for (float v : out.l) REQUIRE(std::abs(v) <= 2);
    for (float v : out.r) REQUIRE(std::abs(v) <= 2);
}
} // namespace
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    test_lengths_and_rates();
    test_frame_and_reset();
    test_dry_and_width();
    test_decay();
    test_freeze();
    test_state_extremes_and_sweep();
    std::puts("abi2_reverb_tests passed");
}
