// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include "comb_body.h"

namespace resonator {
enum Param : uint32_t { Tune, Structure, Decay, Damping, StrikeTone, StrikeLevel, Mix, Level, Count };
constexpr uint32_t kMod = WS2_MODULATABLE | WS2_AUTOMATABLE | WS2_PER_VOICE;
constexpr uint32_t kStep = kMod | WS2_STEPPED;

struct Resonator {
    static constexpr ws::Info info{"essentials.resonator", "Resonator", 1};
    static constexpr std::array inputs{
        ws::port("audio", "Audio", WS2_AUDIO, WS2_POLY),
        ws::port("pitch", "Pitch", WS2_PITCH, WS2_POLY, 0),
        ws::port("strike", "Strike", WS2_TRIGGER, WS2_POLY, 0)};
    static constexpr std::array outputs{ws::port("audio", "Audio", WS2_AUDIO, WS2_POLY)};
    static constexpr std::array params{
        ws::param("tune", "Tune", -24, 24, 0, "st", kMod, WS2_LINEAR, Tune),
        ws::param("structure", "Structure", 0, 3, 0, "", kStep, WS2_LINEAR, Structure, 4, 1),
        ws::param("decay", "Decay", .05f, 20, 1.5f, "s", kMod, WS2_LOG, Decay),
        ws::param("damping", "Damping", 0, 1, .3f, "", kMod, WS2_LINEAR, Damping),
        ws::param("strike_tone", "Strike tone", 0, 1, .5f, "", kMod, WS2_LINEAR, StrikeTone),
        ws::param("strike_level", "Strike level", 0, 1, .8f, "", kMod, WS2_LINEAR, StrikeLevel),
        ws::param("mix", "Mix", 0, 1, 1, "", kMod, WS2_LINEAR, Mix),
        ws::param("level", "Level", 0, 1, .7f, "", kMod, WS2_LINEAR, Level)};

    struct Voice {
        CombBody body;
        ws::Random rng;
        float delay[3]{}, dc_input = 0, dc_output = 0, noise = 0;
        uint32_t burst_left = 0;
        bool high = false, initialized = false;
    };
    std::array<Voice, 8> voices{};
    float rate = 48000.f, glide = 0.f;
    uint32_t capacity = 0, line_size = 0, burst = 0;
    uint64_t seed = 0;

    static uint32_t comb_length(double r) { return static_cast<uint32_t>(std::ceil(r / 20.0)) + 4; }
    static ws2_memory memory_requirements(const ws2_config& c) {
        ws2_memory m{};
        m.struct_size = sizeof(m);
        m.alignment = 32;
        m.large_bytes = size_t(std::min(c.max_channels, 8u)) * (3u * comb_length(c.sample_rate) * sizeof(float) + 32);
        return m;
    }
    bool prepare(const ws::Setup& s) {
        rate = static_cast<float>(s.sample_rate());
        capacity = std::min(s.max_channels(), 8u);
        seed = s.config.seed ^ UINT64_C(0x9e3779b97f4a7c15);
        line_size = comb_length(rate);
        burst = std::max(1u, static_cast<uint32_t>(std::ceil(rate * .005f)));
        glide = 1.f - std::exp(-1.f / (rate * .012f));
        for (uint32_t c = 0; c < capacity; ++c) {
            auto* memory = static_cast<float*>(s.alloc(3u * line_size * sizeof(float), 32, WS2_LARGE));
            if (!memory) return false;
            voices[c].body.attach(memory, line_size);
        }
        reset();
        return true;
    }
    void reset() {
        for (uint32_t c = 0; c < capacity; ++c) {
            auto& v = voices[c];
            v.body.reset();
            v.rng.seed(seed, c);
            std::fill_n(v.delay, 3, 0.f);
            v.dc_input = v.dc_output = v.noise = 0;
            v.burst_left = 0;
            v.high = v.initialized = false;
        }
    }
    void process(ws::Block& b) {
        const uint32_t n = b.channels();
        auto out = b.out(0);
        out.set_channels(n);
        // Even a currently silent comb has running state. Never mark a ringing voice CONSTANT.
        out.mark_constant(false);
        constexpr float ratios[4][3]{
            {1.f, 1.4983f, 2.003f}, {1.f, 2.f, 3.f},
            {1.f, 2.756f, 5.404f}, {1.f, 2.32f, 4.25f}};
        for (uint32_t c = 0; c < n; ++c) {
            float* y = out.ch(c);
            if (c >= capacity) { std::fill_n(y, b.frames(), 0.f); continue; }
            auto& v = voices[c];
            for (uint32_t i = 0; i < b.frames(); ++i) {
                const auto p = [&](uint32_t slot) { return b.param(slot).value(c, i); };
                const float audio = std::clamp(b.in(0).value(c, i), -2.f, 2.f);
                const float trigger = b.in(2).value(c, i);
                const bool high = trigger > .5f ? true : trigger < .25f ? false : v.high;
                if (high && !v.high) v.burst_left = burst;
                v.high = high;
                float strike = 0;
                if (v.burst_left) {
                    const float tone = std::clamp(p(StrikeTone), 0.f, 1.f);
                    const float alpha = .04f + .96f * tone * tone;
                    v.noise += alpha * (v.rng.bipolar() - v.noise);
                    strike = .003f * std::clamp(p(StrikeLevel), 0.f, 1.f) *
                             (float(v.burst_left) / float(burst)) * v.noise;
                    --v.burst_left;
                }
                const float oct = std::clamp(b.in(1).value(c, i) +
                                             std::clamp(p(Tune), -24.f, 24.f) / 12.f, -16.f, 16.f);
                const float hz = std::clamp(261.625565f * std::exp2(oct), 20.f, rate * .45f);
                const uint32_t mode = static_cast<uint32_t>(std::clamp(std::lround(p(Structure)), 0l, 3l));
                for (uint32_t tap = 0; tap < 3; ++tap) {
                    const float target = std::clamp(rate / (hz * ratios[mode][tap]), 2.f, float(line_size - 2));
                    if (!v.initialized) v.delay[tap] = target;
                    else v.delay[tap] += glide * (target - v.delay[tap]);
                }
                v.initialized = true;
                const float seconds = std::clamp(p(Decay), .05f, 20.f);
                float feedback[3];
                for (uint32_t tap = 0; tap < 3; ++tap)
                    feedback[tap] = std::exp(-6.907755279f * v.delay[tap] / (rate * seconds));
                const float body = v.body.process(audio + strike, v.delay, feedback,
                                                  std::clamp(p(Damping), 0.f, 1.f));
                const float mix = std::clamp(p(Mix), 0.f, 1.f);
                const float raw = std::clamp(p(Level), 0.f, 1.f) *
                                  ((1.f - mix) * audio + mix * (16.f * body + .5f * strike));
                // DC block the final signal. The hard ceiling is a safety net for hot exciters.
                const float dc = raw - v.dc_input + .995f * v.dc_output;
                v.dc_input = raw;
                v.dc_output = dc;
                y[i] = std::clamp(dc, -2.f, 2.f);
            }
        }
    }
    // Patch state contains no history: the host persists params, and load clears all tails.
    int save_state(ws::Writer w) { return w.u32(1u) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1u) return WS2_UNSUPPORTED;
        reset();
        return WS2_OK;
    }
};
} // namespace resonator
