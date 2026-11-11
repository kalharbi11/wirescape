// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace reverb {
enum Param : uint8_t { SIZE, DECAY, DAMPING, PREDELAY, WIDTH, MOTION, FREEZE, MIX, COUNT };
constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfPi = 1.57079632679489661923f;
constexpr float kLn1000 = 6.907755278982137f;
constexpr uint32_t kMod = WS2_MODULATABLE | WS2_AUTOMATABLE;

// A Schroeder allpass, with host-owned storage. Its transfer is unity magnitude.
struct Diffuser {
    float *samples = nullptr;
    uint32_t count = 0, cursor = 0;
    void attach(float *p, uint32_t n) {
        samples = p;
        count = n;
    }
    void clear() {
        if (samples) std::fill_n(samples, count, 0.f);
        cursor = 0;
    }
    float step(float input) {
        constexpr float g = .62f;
        const float delayed = samples[cursor];
        const float out = delayed - g * input;
        samples[cursor] = input + g * out;
        if (++cursor == count) cursor = 0;
        return out;
    }
};

struct Reverb {
    static constexpr ws::Info info{"fx.reverb", "Reverb", 1};
    static constexpr std::array inputs{ws::port("left", "In L", WS2_AUDIO, WS2_SUM, 0, "stereo"),
                                       ws::port("right", "In R", WS2_AUDIO, WS2_SUM, 0, "stereo")};
    static constexpr std::array outputs{ws::port("left", "Out L", WS2_AUDIO, WS2_POLY, 0, "stereo"),
                                        ws::port("right", "Out R", WS2_AUDIO, WS2_POLY, 0, "stereo")};
    static constexpr std::array params{
        ws::param("size", "Size", 0, 1, .5f, "", kMod, WS2_LINEAR, SIZE),
        ws::param("decay", "Decay", .2f, 20, 2.5f, "s", kMod, WS2_LOG, DECAY),
        ws::param("damping", "Damping", 0, 1, .4f, "", kMod, WS2_LINEAR, DAMPING),
        ws::param("predelay", "Pre-delay", 0, .25f, .02f, "s", kMod, WS2_LINEAR, PREDELAY),
        ws::param("width", "Width", 0, 1, 1, "", kMod, WS2_LINEAR, WIDTH),
        ws::param("motion", "Motion", 0, 1, .2f, "", kMod, WS2_LINEAR, MOTION),
        ws::param("freeze", "Freeze", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, FREEZE, 4, 1),
        ws::param("mix", "Mix", 0, 1, .3f, "", kMod, WS2_LINEAR, MIX)};

    static constexpr std::array<uint32_t, 8> base{{1499, 1741, 1999, 2281, 2617, 3011, 3469, 4789}};
    static constexpr std::array<uint32_t, 4> diffusion_base{{251, 397, 307, 449}};
    // Orthogonal output projections; the independent delay lengths further decorrelate them.
    static constexpr std::array<float, 8> project_l{{1, 1, 1, 1, -1, -1, -1, -1}};
    static constexpr std::array<float, 8> project_r{{1, -1, 1, -1, 1, -1, 1, -1}};

    float rate = 48000;
    uint32_t line_capacity = 0, pre_capacity = 0, write = 0, pre_write = 0;
    float *lines[8]{};
    float *pre[2]{};
    Diffuser diffusion[4]{};
    double delay[8]{}; // Float glides stall before the integer target, making frozen feedback lossy.
    float target[8]{}, gain[8]{}, low[8]{};
    float pre_samples = 960, freeze_amount = 0, phase_a = 0, phase_b = 0;
    static constexpr float kWetMakeup = 5.f;
    float last_mix = -1.f, dry_gain = 1.f, wet_gain = 0.f, last_size = -1.f;
    uint64_t sample_counter = 0;

    static uint32_t line_count(double sr) { return uint32_t(std::ceil(sr * .103)) + 8; }
    static uint32_t pre_count(double sr) { return uint32_t(std::ceil(sr * .25)) + 8; }
    static uint32_t diffusion_count(double sr, uint32_t k) {
        return uint32_t(std::ceil(double(diffusion_base[k]) * sr / 48000.0)) + 2;
    }
    static ws2_memory memory_requirements(const ws2_config &c) {
        ws2_memory m{};
        m.struct_size = sizeof(m);
        m.alignment = 32;
        const size_t samples = size_t(8) * line_count(c.sample_rate) + size_t(2) * pre_count(c.sample_rate) +
                               diffusion_count(c.sample_rate, 0) + diffusion_count(c.sample_rate, 1) +
                               diffusion_count(c.sample_rate, 2) + diffusion_count(c.sample_rate, 3);
        m.large_bytes = samples * sizeof(float) + size_t(14) * 32; // separate aligned host allocations
        return m;
    }
    static bool prime(uint32_t x) {
        if (x < 2) return false;
        for (uint32_t i = 2; i * i <= x; ++i)
            if (x % i == 0) return false;
        return true;
    }
    static uint32_t nearest_prime(uint32_t x) {
        x = std::max<uint32_t>(53, x);
        for (uint32_t offset = 0;; ++offset) {
            if (x > offset && prime(x - offset)) return x - offset;
            if (prime(x + offset)) return x + offset;
        }
    }
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- a line index and normalized size are distinct.
    uint32_t scaled_length(uint32_t k, float size) const {
        const double factor = .25 + .75 * std::clamp(size, 0.f, 1.f);
        return nearest_prime(uint32_t(std::lround(double(base[k]) * double(rate) * factor / 48000.0)));
    }
    bool prepare(const ws::Setup &s) {
        rate = float(s.sample_rate());
        line_capacity = line_count(s.sample_rate());
        pre_capacity = pre_count(s.sample_rate());
        for (auto &p : lines) {
            p = static_cast<float *>(s.alloc(size_t(line_capacity) * sizeof(float), 32, WS2_LARGE));
            if (!p) return false;
        }
        for (auto &p : pre) {
            p = static_cast<float *>(s.alloc(size_t(pre_capacity) * sizeof(float), 32, WS2_LARGE));
            if (!p) return false;
        }
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t n = diffusion_count(s.sample_rate(), i);
            auto *p = static_cast<float *>(s.alloc(size_t(n) * sizeof(float), 32, WS2_LARGE));
            if (!p) return false;
            diffusion[i].attach(p, n);
        }
        return true;
    }
    void reset() {
        write = pre_write = 0;
        sample_counter = 0;
        freeze_amount = phase_a = phase_b = 0;
        pre_samples = .02f * rate;
        last_mix = last_size = -1.f;
        for (uint32_t k = 0; k < 8; ++k) {
            if (lines[k]) std::fill_n(lines[k], line_capacity, 0.f);
            delay[k] = target[k] = float(scaled_length(k, .5f));
            gain[k] = .9f;
            low[k] = 0;
        }
        for (auto *p : pre)
            if (p) std::fill_n(p, pre_capacity, 0.f);
        for (auto &stage : diffusion) stage.clear();
    }
    static void hadamard(float *v) {
        constexpr float a = .7071067811865475f;
        for (uint32_t stride = 1; stride < 8; stride *= 2)
            for (uint32_t start = 0; start < 8; start += 2 * stride)
                for (uint32_t i = start; i < start + stride; ++i) {
                    const float x = v[i], y = v[i + stride];
                    v[i] = (x + y) * a;
                    v[i + stride] = (x - y) * a;
                }
    }
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- ring capacity and write cursor are distinct.
    float tap(const float *data, uint32_t capacity, uint32_t cursor, float length) const {
        const uint32_t n = uint32_t(length);
        const float frac = length - float(n);
        const uint32_t at = cursor >= n ? cursor - n : cursor + capacity - n;
        const uint32_t before = at ? at - 1 : capacity - 1;
        return data[at] + frac * (data[before] - data[at]);
    }
    void refresh(const ws::Block &b, uint32_t i) {
        const float size = b.param(SIZE).value(0, i);
        if (size != last_size) {
            last_size = size;
            for (uint32_t k = 0; k < 8; ++k) {
                target[k] = float(std::min(line_capacity - 3, scaled_length(k, size)));
            }
        }
        const float rt60 = std::clamp(b.param(DECAY).value(0, i), .2f, 20.f);
        for (uint32_t k = 0; k < 8; ++k) gain[k] = float(std::exp(-kLn1000 * delay[k] / (rt60 * rate)));
    }
    void process(ws::Block &b) {
        auto lout = b.out(0), rout = b.out(1);
        lout.set_channels(1);
        rout.set_channels(1);
        lout.mark_constant(false);
        rout.mark_constant(false); // even silence advances the tail
        const bool right_connected = b.in(1).connected();
        const float glide = 1.f / (rate * .05f);
        const float freeze_step = 1.f / (rate * .02f);
        for (uint32_t i = 0; i < b.frames(); ++i, ++sample_counter) {
            if ((sample_counter & 63u) == 0) refresh(b, i); // absolute, block-size independent
            const float frozen_target = b.param(FREEZE).value(0, i) >= .5f ? 1.f : 0.f;
            freeze_amount += std::clamp(frozen_target - freeze_amount, -freeze_step, freeze_step);
            const float damp = std::clamp(b.param(DAMPING).value(0, i), 0.f, 1.f) * (1.f - freeze_amount);
            const float motion = std::clamp(b.param(MOTION).value(0, i), 0.f, 1.f) * (1.f - freeze_amount);
            const float mix = std::clamp(b.param(MIX).value(0, i), 0.f, 1.f);
            const float width = std::clamp(b.param(WIDTH).value(0, i), 0.f, 1.f);
            const float l = b.in(0).value(0, i);
            const float r = right_connected ? b.in(1).value(0, i) : l;
            const float predelay_target = std::clamp(b.param(PREDELAY).value(0, i), 0.f, .25f) * rate;
            pre_samples += glide * (predelay_target - pre_samples);
            if (std::abs(pre_samples - predelay_target) < .001f) pre_samples = predelay_target;
            const float drive = 1.f - freeze_amount;
            float d[2] = {l * drive, r * drive};
            for (uint32_t c = 0; c < 2; ++c) {
                const float old = pre_samples < 1 ? d[c]
                                                  : tap(pre[c], pre_capacity, pre_write,
                                                        std::clamp(pre_samples, 1.f, float(pre_capacity - 2)));
                pre[c][pre_write] = d[c];
                d[c] = old;
                d[c] = diffusion[size_t(c) * 2 + 1].step(diffusion[size_t(c) * 2].step(d[c]));
            }
            if (++pre_write == pre_capacity) pre_write = 0;

            phase_a += .17f / rate;
            if (phase_a >= 1) phase_a -= 1;
            phase_b += .23f / rate;
            if (phase_b >= 1) phase_b -= 1;
            const float ma = motion == 0.f ? 0.f : motion * std::sin(2.f * kPi * phase_a);
            const float mb = motion == 0.f ? 0.f : motion * std::sin(2.f * kPi * phase_b);
            float vector[8], wet_l = 0, wet_r = 0;
            for (uint32_t k = 0; k < 8; ++k) {
                delay[k] += glide * (target[k] - delay[k]);
                if (std::abs(delay[k] - target[k]) < .001f) delay[k] = target[k];
                const float modulated = float(delay[k]) + (k & 1 ? ma : -ma) + (k & 2 ? mb : -mb);
                const float shifted = std::clamp(modulated, 2.f, float(line_capacity - 2));
                const float delayed = tap(lines[k], line_capacity, write, shifted);
                wet_l += project_l[k] * delayed;
                wet_r += project_r[k] * delayed;
                low[k] += (delayed - low[k]) * (1.f - damp);
                const float feedback = gain[k] + freeze_amount * (1.f - gain[k]);
                vector[k] = (low[k] * (1.f - freeze_amount) + delayed * freeze_amount) * feedback;
            }
            hadamard(vector);
            for (uint32_t k = 0; k < 8; ++k) {
                const float injection = .20f * (k & 1 ? d[1] : d[0]) * (k & 4 ? -1.f : 1.f);
                // Normally inactive; bounds pathological feedback without nonlinearising small tails.
                lines[k][write] = std::clamp(vector[k] + injection, -8.f, 8.f);
            }
            if (++write == line_capacity) write = 0;
            wet_l *= .12374369f;
            wet_r *= .12374369f;
            const float mid = .5f * (wet_l + wet_r);
            wet_l = mid + width * (wet_l - mid);
            wet_r = mid + width * (wet_r - mid);
            if (mix == 0.f) {
                lout.ch(0)[i] = l;
                rout.ch(0)[i] = r;
                continue;
            }
            // Compute trigonometric gains only when the sample-accurate Mix value changes.
            if (mix != last_mix) {
                last_mix = mix;
                dry_gain = mix == 1.f ? 0.f : std::cos(mix * kHalfPi);
                // The tank output sits ~16 dB below the dry input, so an equal-power
                // law alone made high Mix settings collapse (owner v1 report). Wet
                // makeup grows with mix^2: about +3 dB of wet at 0.3, +15.6 dB at 1.
                const float makeup = 1.f + kWetMakeup * mix * mix;
                wet_gain = (mix == 1.f ? 1.f : std::sin(mix * kHalfPi)) * makeup;
            }
            lout.ch(0)[i] = std::clamp(dry_gain * l + wet_gain * wet_l, -2.f, 2.f);
            rout.ch(0)[i] = std::clamp(dry_gain * r + wet_gain * wet_r, -2.f, 2.f);
        }
    }
    // Host owns all parameters. Patch loads intentionally discard delay/filter history.
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;
        reset();
        return WS2_OK;
    }
};
} // namespace reverb
