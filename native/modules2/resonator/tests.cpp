// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {
using resonator::Resonator;
constexpr uint32_t kFrames = 256, kChannels = 16;
constexpr double kRate = 48000., kC4 = 261.625565, kPi2 = 6.283185307179586;
struct Arena {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(1024 * 1024);
    size_t used = 0;
    static void* WS2_CALL alloc(void* ptr, size_t count, size_t alignment, uint32_t) {
        auto& a = *static_cast<Arena*>(ptr);
        const uintptr_t base = reinterpret_cast<uintptr_t>(a.bytes.data());
        const uintptr_t at = (base + a.used + alignment - 1) & ~(uintptr_t(alignment) - 1);
        a.used = at - base + count;
        return a.used > a.bytes.size() ? nullptr : reinterpret_cast<void*>(at);
    }
};
struct Buffers {
    float in[3][kChannels][kFrames]{};
    float out[kChannels][kFrames]{};
};
enum class Exciter { None, Strike, Impulse, Noise };
struct Rig {
    Arena arena;
    ws2_host host{sizeof(host), &arena, Arena::alloc};
    const ws2_descriptor* d = ws::Module<Resonator>::descriptor();
    void* instance = nullptr;
    std::unique_ptr<Buffers> buffer = std::make_unique<Buffers>();
    const float* input_rows[3][kChannels]{};
    float* output_rows[kChannels]{};
    ws2_signal inputs[3]{};
    ws2_signal_out outputs[1]{};
    float params_values[resonator::Count]{};
    const float* param_rows[resonator::Count]{};
    ws2_signal params[resonator::Count]{};
    ws2_process ctx{};
    float pitch[kChannels]{};

    explicit Rig(double rate = kRate, uint32_t channels = 1) {
        const ws2_config config{sizeof(config), rate, kFrames, kChannels, 42, nullptr, 0};
        instance = d->create(&host);
        REQUIRE(instance && d->prepare(instance, &config, &host) == WS2_OK);
        for (uint32_t p = 0; p < 3; ++p) {
            for (uint32_t c = 0; c < kChannels; ++c)
                input_rows[p][c] = buffer->in[p][c];
            inputs[p] = {sizeof(ws2_signal), input_rows[p], channels, WS2_CONNECTED};
        }
        for (uint32_t c = 0; c < kChannels; ++c) output_rows[c] = buffer->out[c];
        outputs[0] = {sizeof(ws2_signal_out), output_rows, channels, 0, kChannels};
        for (uint32_t p = 0; p < resonator::Count; ++p) {
            params_values[p] = Resonator::params[p].initial;
            param_rows[p] = &params_values[p];
            params[p] = {sizeof(ws2_signal), &param_rows[p], 1, WS2_CONSTANT};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = channels;
        ctx.input_count = 3;
        ctx.output_count = 1;
        ctx.param_count = resonator::Count;
        ctx.inputs = inputs;
        ctx.outputs = outputs;
        ctx.params = params;
    }
    ~Rig() { d->destroy(instance); }
    Rig& set(resonator::Param p, float v) { params_values[p] = v; return *this; }
    void reset() { REQUIRE(d->reset(instance) == WS2_OK); }

    // Planar output: channel * total + absolute frame. The input generator is
    // indexed by absolute frame, so host block cuts cannot affect excitation.
    std::vector<float> render(uint32_t total, uint32_t block, Exciter e, uint32_t at = 0) {
        std::vector<float> y(size_t(ctx.channels) * total);
        for (uint32_t first = 0; first < total; first += block) {
            const uint32_t n = std::min(block, total - first);
            for (uint32_t c = 0; c < ctx.channels; ++c) {
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t t = first + i;
                    const uint32_t bits = t * 1664525u + (c + 1u) * 1013904223u;
                    buffer->in[0][c][i] = e == Exciter::Impulse && t == at ? 1.f :
                        e == Exciter::Noise ? (float(bits & 65535u) / 32767.5f - 1.f) : 0.f;
                    buffer->in[1][c][i] = pitch[c];
                    buffer->in[2][c][i] = e == Exciter::Strike && t >= at && t < at + 4 ? 1.f : 0.f;
                }
            }
            ctx.frames = n;
            int result;
            { allocation_watch::Scope watch;
              result = d->process(instance, &ctx);
              REQUIRE(watch.allocations() == 0); }
            REQUIRE(result == WS2_OK && outputs[0].channels == ctx.channels);
            REQUIRE(!(outputs[0].flags & WS2_CONSTANT));
            for (uint32_t c = 0; c < ctx.channels; ++c)
                for (uint32_t i = 0; i < n; ++i) {
                    const float sample = buffer->out[c][i];
                    REQUIRE(std::isfinite(sample) && std::abs(sample) <= 2.f);
                    if (c >= 8) REQUIRE(sample == 0.f);
                    y[size_t(c) * total + first + i] = sample;
                }
            ctx.frame_position += n;
        }
        return y;
    }
    Bytes save() {
        Bytes data;
        ws2_writer w{sizeof(w), &data, sink};
        REQUIRE(d->save_state(instance, &w) == WS2_OK);
        return data;
    }
    int load(const Bytes& data) {
        Source src{&data, 0};
        ws2_reader r{sizeof(r), &src, source, uint32_t(data.size())};
        return d->load_state(instance, &r);
    }
};
double energy(const std::vector<float>& y, uint32_t total, uint32_t channel,
              uint32_t begin, uint32_t end, double rate, double hz) {
    const double coeff = 2. * std::cos(kPi2 * hz / rate);
    double a = 0, b = 0;
    for (uint32_t i = begin; i < end; ++i) {
        const double v = y[size_t(channel) * total + i] + coeff * a - b;
        b = a; a = v;
    }
    return std::max(0., a * a + b * b - coeff * a * b);
}
double strongest(const std::vector<float>& y, uint32_t total, uint32_t channel,
                 uint32_t begin, uint32_t end, double rate,
                 double lo, double hi, double step = 1.) {
    double best_hz = lo, best = -1.;
    for (double hz = lo; hz <= hi; hz += step) {
        const double e = energy(y, total, channel, begin, end, rate, hz);
        if (e > best) { best_hz = hz; best = e; }
    }
    return best_hz;
}
double rms(const std::vector<float>& y, uint32_t begin, uint32_t end) {
    double sum = 0;
    for (uint32_t i = begin; i < end; ++i) sum += double(y[i]) * y[i];
    return std::sqrt(sum / double(end - begin));
}
void matrix_and_reset() {
    for (double rate : {44100., 48000., 96000.})
        for (uint32_t channels : {1u, 4u, 16u}) {
            Rig whole(rate, channels);
            auto expected = whole.render(1025, 256, Exciter::Strike, 37);
            REQUIRE(std::any_of(expected.begin(), expected.begin() + 1025,
                                [](float x) { return x != 0; }));
            for (uint32_t block : {1u, 17u, 64u}) {
                Rig cut(rate, channels);
                REQUIRE(cut.render(1025, block, Exciter::Strike, 37) == expected);
            }
            whole.reset();
            REQUIRE(whole.render(1025, 17, Exciter::Strike, 37) == expected);
        }
}
void onset_and_external() {
    Rig quiet;
    for (float x : quiet.render(400, 17, Exciter::None)) REQUIRE(x == 0.f);
    Rig struck;
    const auto y = struck.render(1200, 17, Exciter::Strike, 73);
    for (uint32_t i = 0; i < 73; ++i) REQUIRE(y[i] == 0.f);
    REQUIRE(y[73] != 0.f);   // no off-by-one block latency
    REQUIRE(std::any_of(y.begin() + 200, y.end(), [](float x) { return std::abs(x) > 1.e-6f; }));
    Rig ext;
    ext.set(resonator::Structure, 1).set(resonator::Damping, 0);
    const auto impulse = ext.render(16384, 256, Exciter::Impulse);
    REQUIRE(std::abs(strongest(impulse, 16384, 0, 2048, 16384, kRate,
                              240, 285, .5) - kC4) < kC4 * .01);
}
void modes_and_poly() {
    constexpr uint32_t total = 16384;
    Rig harmonic;
    harmonic.set(resonator::Structure, 1).set(resonator::Damping, 0);
    const auto h = harmonic.render(total, 256, Exciter::Strike);
    REQUIRE(std::abs(strongest(h, total, 0, 1024, total, kRate, 200, 860) - kC4) < kC4 * .01);
    Rig body;
    body.set(resonator::Structure, 0).set(resonator::Damping, 0);
    const auto b = body.render(total, 256, Exciter::Strike);
    for (double ratio : {1., 1.4983, 2.003}) {
        const double hz = kC4 * ratio;
        const double found = strongest(b, total, 0, 2048, total, kRate, hz * .98, hz * 1.02, .5);
        REQUIRE(std::abs(found / hz - 1.) < .01);
        REQUIRE(energy(b, total, 0, 2048, total, kRate, found) >
                energy(b, total, 0, 2048, total, kRate, hz * 1.07));
    }
    Rig poly(kRate, 16);
    poly.pitch[0] = 0; poly.pitch[1] = -1;
    const auto v = poly.render(total, 256, Exciter::Strike);
    REQUIRE(std::abs(strongest(v, total, 0, 2048, total, kRate, 245, 280) - kC4) < kC4 * .01);
    REQUIRE(std::abs(strongest(v, total, 1, 2048, total, kRate, 120, 145) - kC4 / 2) < kC4 * .01);
    Rig mono(kRate, 2);
    mono.inputs[0].channels = 1;
    mono.inputs[1].channels = 1;
    const auto m = mono.render(2048, 17, Exciter::Impulse);
    for (uint32_t i = 0; i < 2048; ++i) REQUIRE(m[i] == m[2048 + i]);
}
void decay_t60() {
    for (float seconds : {.5f, 4.f}) {
        Rig r;
        r.set(resonator::Structure, 1).set(resonator::Decay, seconds)
         .set(resonator::Damping, 0).set(resonator::StrikeTone, 1);
        const uint32_t first = uint32_t(.15 * kRate);
        const uint32_t offset = uint32_t(seconds * kRate);
        const uint32_t win = uint32_t(.04 * kRate);
        const uint32_t total = first + offset + win + 256;
        const auto y = r.render(total, 256, Exciter::Strike);
        const double a = rms(y, first, first + win);
        const double b = rms(y, first + offset, first + offset + win);
        REQUIRE(a > 1.e-8 && b > 0 && b < a);
        const double measured = double(seconds) * std::log(.001) / std::log(b / a);
        std::printf("resonator T60 %.2f s measured %.3f s\n", double(seconds), measured);
        REQUIRE(measured > seconds * .75 && measured < seconds * 1.25);
    }
}
void states_extremes_and_glide() {
    Rig used;
    used.render(2000, 64, Exciter::Strike);
    const Bytes state = used.save();
    REQUIRE(state.size() == 4);
    Bytes bad = state; bad[0] = 77;
    REQUIRE(used.load(bad) == WS2_UNSUPPORTED);
    Bytes short_data;
    REQUIRE(used.load(short_data) == WS2_INVALID);
    REQUIRE(used.load(state) == WS2_OK);
    Rig fresh;
    REQUIRE(used.render(2000, 17, Exciter::Strike) ==
            fresh.render(2000, 17, Exciter::Strike));
    for (uint32_t extreme = 0; extreme < 2; ++extreme) {
        Rig r(96000, 16);
        for (uint32_t p = 0; p < resonator::Count; ++p)
            r.params_values[p] = extreme ? Resonator::params[p].maximum : Resonator::params[p].minimum;
        r.render(4096, 256, Exciter::Noise);
    }
    Rig hot;
    hot.set(resonator::Decay, 20).set(resonator::Damping, 0);
    hot.render(30u * 48000u, 256, Exciter::Noise); // 30s, finite and bounded
    Rig glide;
    glide.render(4096, 256, Exciter::Strike);
    glide.pitch[0] = -2;
    glide.set(resonator::Structure, 3);
    const auto shifted = glide.render(4096, 64, Exciter::None);
    for (size_t i = 1; i < shifted.size(); ++i)
        REQUIRE(std::abs(shifted[i] - shifted[i - 1]) < .5f);
}
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    matrix_and_reset();
    onset_and_external();
    modes_and_poly();
    decay_t60();
    states_extremes_and_glide();
    std::puts("abi2_resonator_tests passed");
    return 0;
}
