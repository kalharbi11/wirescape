// SPDX-License-Identifier: GPL-3.0-only
// fx.delay: stereo delay. Original code; see DESIGN.md.
#pragma once
#include "wirescape_sdk.h"
#include <algorithm>
#include <cmath>

namespace fxdelay {
struct Delay {
    static constexpr ws::Info info{"fx.delay", "Delay", 1};
    static constexpr std::array inputs{ws::port("left", "In L", WS2_AUDIO, WS2_SUM, 0, "stereo"),
                                       ws::port("right", "In R", WS2_AUDIO, WS2_SUM, 0, "stereo"),
                                       ws::port("clock", "Clock", WS2_TRIGGER, WS2_FIRST)};
    static constexpr std::array outputs{ws::port("left", "Out L", WS2_AUDIO, WS2_POLY, 0, "stereo"),
                                        ws::port("right", "Out R", WS2_AUDIO, WS2_POLY, 0, "stereo")};
    static constexpr uint32_t stepped = WS2_STEPPED | WS2_AUTOMATABLE;
    static constexpr std::array params{
        ws::param("time", "Time", 0.001f, 2.f, 0.375f, "s", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LOG, 0),
        ws::param("division", "Division", 0, 7, 5, "", stepped, WS2_LINEAR, 1, 4, 1),
        ws::param("feedback", "Feedback", 0, 1, 0.4f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 2),
        ws::param("tone", "Tone", -1, 1, 0, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 3),
        ws::param("mode", "Mode", 0, 1, 0, "", stepped, WS2_LINEAR, 4, 4, 1),
        ws::param("mix", "Mix", 0, 1, 0.35f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 5)};
    enum Slot : uint8_t { kTime, kDivision, kFeedback, kTone, kMode, kMix };

    static constexpr double kMax = 2.0, kMin = 0.001;   // seconds
    static constexpr double kGlide = 0.05, kSlew = 0.5; // glide time constant; max samples moved per sample
    static constexpr float kKnee = 1.f, kCeiling = 1.5f, kWetMax = 1.6f;
    static constexpr double kDivisions[8]{0.25, 1.0 / 3, 0.5, 2.0 / 3, 0.75, 1.0, 1.5, 2.0};

    float *line[2]{};
    uint32_t size = 0, w = 0;
    double rate = 48000, cur = 0, alpha = 0;
    float dc_r = 0.999f, lp[2]{}, dc_x[2]{}, dc_y[2]{};
    bool primed = false;
    // tone cache (a pure function of the tone parameter)
    float tone_seen = 9.f, tone_a = 0, tone_k = 0;
    int tone_kind = 0;
    // clock follower
    ws::Schmitt schmitt;
    bool high = false, seen = false;
    uint64_t since = 0;
    double applied = 0, pending = 0;

    static uint32_t length(double r) { return static_cast<uint32_t>(std::ceil(kMax * r)) + 8; }
    static ws2_memory memory_requirements(const ws2_config &c) {
        ws2_memory m{};
        m.struct_size = sizeof(m);
        m.alignment = 32;
        m.large_bytes = 2 * (size_t(length(c.sample_rate)) * sizeof(float) + 64);
        return m;
    }
    bool prepare(const ws::Setup &s) {
        rate = s.sample_rate();
        size = length(rate);
        for (auto &l : line) {
            l = static_cast<float *>(s.alloc(size * sizeof(float), 32, WS2_LARGE));
            if (!l) return false;
        }
        alpha = 1.0 - std::exp(-1.0 / (kGlide * rate));
        dc_r = static_cast<float>(1.0 - 6.283185307179586 * 10.0 / rate);
        reset();
        return true;
    }
    void reset() {
        for (auto *l : line)
            if (l) std::fill_n(l, size, 0.f);
        w = 0;
        cur = 0;
        primed = false;
        for (int i = 0; i < 2; ++i) lp[i] = dc_x[i] = dc_y[i] = 0.f;
        clock_reset();
    }
    void clock_reset() {
        schmitt.reset();
        high = seen = false;
        since = 0;
        applied = pending = 0;
    }
    int save_state(ws::Writer out) const { return out.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader in) {
        uint32_t version = 0;
        if (!in.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;
        reset(); // delay lines clear on load
        return WS2_OK;
    }

    static float sum(const ws::Signal &s, uint32_t i) {
        float t = 0;
        for (uint32_t c = 0; c < s.channels(); ++c) t += s.ch(c)[i];
        return t;
    }
    // Soft limiter: identity up to the knee, then tanh-compressed towards the ceiling (1.5).
    static float limit(float x) {
        const float a = std::fabs(x);
        if (a <= kKnee) return x;
        return std::copysign(kKnee + (kCeiling - kKnee) * std::tanh((a - kKnee) / (kCeiling - kKnee)), x);
    }
    static float flush(float x) { return std::fabs(x) < 1e-18f ? 0.f : x; }
    // 4-point Hermite (Catmull-Rom) read `d` samples behind the write position (2 <= d <= 2 s).
    float read(const float *buf, double d) const {
        const int di = static_cast<int>(d);
        const float fr = static_cast<float>(d - di);
        auto at = [&](int age) {
            int k = static_cast<int>(w) - age;
            if (k < 0) k += static_cast<int>(size);
            return buf[k];
        };
        const float y1 = at(di);
        if (fr == 0.f) return y1;
        const float ym1 = at(di + 2), y0 = at(di + 1), y2 = at(di - 1), mu = 1.f - fr;
        const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * mu + c2) * mu + c1) * mu + y0;
    }
    void update_tone(float tone) {
        if (tone == tone_seen) return;
        tone_seen = tone;
        const double t = std::clamp(static_cast<double>(tone), -1.0, 1.0);
        double fc = 0;
        if (std::fabs(t) < 1e-6) {
            tone_kind = 0;
        } else if (t < 0) {
            tone_kind = -1;
            fc = 800.0 * std::pow(25.0, 1.0 + t); // low-pass 20 kHz .. 800 Hz
        } else {
            tone_kind = 1;
            fc = 20.0 * std::pow(75.0, t); // high-pass 20 Hz .. 1.5 kHz
        }
        fc = std::min(fc, 0.45 * rate);
        tone_a = static_cast<float>(1.0 - std::exp(-6.283185307179586 * fc / rate));
        tone_k = static_cast<float>(std::min(1.0, 4.0 * std::fabs(t))); // fades in, so tone 0 is exactly flat
    }
    // Feedback path of one line: tone filter, then DC blocker.
    float loop(int ch, float x) {
        float y = x;
        if (tone_kind) {
            lp[ch] = flush(lp[ch] + tone_a * (x - lp[ch]));
            const float f = tone_kind < 0 ? lp[ch] : x - lp[ch];
            y = x + tone_k * (f - x);
        }
        const float out = flush(y - dc_x[ch] + dc_r * dc_y[ch]);
        dc_x[ch] = y;
        dc_y[ch] = out;
        return out;
    }
    // Measures the clock period from rising edges. The first interval applies at the second edge; a later change
    // needs two consistent intervals. No edge for 4 s: keep the last period and restart measuring.
    void clock_tick(float v) {
        const bool now = schmitt.process(v);
        const bool edge = now && !high;
        high = now;
        ++since;
        if (edge) {
            if (seen) {
                const double iv = static_cast<double>(since);
                auto same = [](double a, double b) { return std::fabs(a - b) <= 0.02 * std::max(a, b); };
                if (applied == 0 || same(iv, applied) || (pending > 0 && same(iv, pending))) {
                    applied = iv;
                    pending = 0;
                } else {
                    pending = iv;
                }
            }
            seen = true;
            since = 0;
        } else if (seen && static_cast<double>(since) > 4.0 * rate) {
            seen = false;
            pending = 0;
        }
    }

    void process(ws::Block &b) {
        for (uint32_t o = 0; o < 2; ++o) {
            b.out(o).set_channels(1);
            b.out(o).mark_constant(false);
        }
        float *out[2]{b.out(0).ch(0), b.out(1).ch(0)};
        const auto in_l = b.in(0), in_r = b.in(1), clock = b.in(2);
        const bool right_patched = in_r.connected(), clocked = clock.connected();
        if (!clocked) clock_reset();
        const auto p_time = b.param(kTime), p_div = b.param(kDivision), p_fb = b.param(kFeedback);
        const auto p_tone = b.param(kTone), p_mode = b.param(kMode), p_mix = b.param(kMix);
        const bool mix_constant = p_mix.constant();
        float dry = 0, wet = 0;
        const double lo = kMin * rate, hi = kMax * rate;
        for (uint32_t i = 0; i < b.frames(); ++i) {
            const float xl = sum(in_l, i), xr = right_patched ? sum(in_r, i) : xl;
            if (clocked) clock_tick(clock.ch(0)[i]);
            double target = static_cast<double>(p_time.ch(0)[i]) * rate;
            if (clocked && applied > 0) {
                const long d = std::clamp(std::lround(p_div.ch(0)[i]), 0l, 7l);
                target = applied * kDivisions[d];
            }
            target = std::clamp(target, lo, hi);
            if (!primed) {
                cur = target;
                primed = true;
            } else {
                const double diff = target - cur;
                cur += std::fabs(diff) < 1e-4 ? diff : std::clamp(diff * alpha, -kSlew, kSlew);
            }
            const float dl = std::clamp(read(line[0], cur), -kWetMax, kWetMax);
            const float dr = std::clamp(read(line[1], cur), -kWetMax, kWetMax);
            update_tone(p_tone.ch(0)[i]);
            const float fb = std::clamp(p_fb.ch(0)[i], 0.f, 1.f);
            const float fl = loop(0, dl), fr = loop(1, dr);
            if (std::lround(p_mode.ch(0)[i]) >= 1) { // ping-pong: mono in, bounce L -> R -> L
                line[0][w] = limit(0.5f * (xl + xr) + fb * fr);
                line[1][w] = limit(fb * fl);
            } else {
                line[0][w] = limit(xl + fb * fl);
                line[1][w] = limit(xr + fb * fr);
            }
            if (++w == size) w = 0;
            if (i == 0 || !mix_constant) {
                const double m = std::clamp(static_cast<double>(p_mix.ch(0)[i]), 0.0, 1.0);
                dry = m >= 1.0 ? 0.f : static_cast<float>(std::cos(m * 1.5707963267948966));
                wet = m <= 0.0 ? 0.f : static_cast<float>(std::sin(m * 1.5707963267948966));
            }
            out[0][i] = dry * xl + wet * dl;
            out[1][i] = dry * xr + wet * dr;
        }
    }
};
} // namespace fxdelay
