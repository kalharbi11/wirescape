// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace quad_vca {
struct QuadVca {
    static constexpr ws::Info info{"essentials.quad_vca", "Quad VCA", 1};
    static constexpr std::array inputs{ws::port("in1", "Input 1", WS2_AUDIO),
                                       ws::port("in2", "Input 2", WS2_AUDIO),
                                       ws::port("in3", "Input 3", WS2_AUDIO),
                                       ws::port("in4", "Input 4", WS2_AUDIO),
                                       ws::port("cv1", "CV 1", WS2_CV_UNI, WS2_POLY, 1),
                                       ws::port("cv2", "CV 2", WS2_CV_UNI, WS2_POLY, 1),
                                       ws::port("cv3", "CV 3", WS2_CV_UNI, WS2_POLY, 1),
                                       ws::port("cv4", "CV 4", WS2_CV_UNI, WS2_POLY, 1)};
    static constexpr uint32_t kLevel = WS2_MODULATABLE | WS2_PER_VOICE | WS2_AUDIO_RATE_OK | WS2_AUTOMATABLE;
    static constexpr uint32_t kMode = WS2_STEPPED | WS2_AUTOMATABLE;
    static constexpr std::array outputs{ws::port("out1", "Out 1", WS2_AUDIO),
                                        ws::port("out2", "Out 2", WS2_AUDIO),
                                        ws::port("out3", "Out 3", WS2_AUDIO),
                                        ws::port("out4", "Out 4", WS2_AUDIO),
                                        ws::port("mix", "Mix", WS2_AUDIO)};
    static constexpr std::array params{
        ws::param("level1", "Level 1", 0, 1, 1, "", kLevel, WS2_LINEAR, 0),
        ws::param("level2", "Level 2", 0, 1, 1, "", kLevel, WS2_LINEAR, 1),
        ws::param("level3", "Level 3", 0, 1, 1, "", kLevel, WS2_LINEAR, 2),
        ws::param("level4", "Level 4", 0, 1, 1, "", kLevel, WS2_LINEAR, 3),
        ws::param("mode1", "Mode 1", 0, 1, 0, "", kMode, WS2_LINEAR, 4, 4, 1),
        ws::param("mode2", "Mode 2", 0, 1, 0, "", kMode, WS2_LINEAR, 5, 4, 1),
        ws::param("mode3", "Mode 3", 0, 1, 0, "", kMode, WS2_LINEAR, 6, 4, 1),
        ws::param("mode4", "Mode 4", 0, 1, 0, "", kMode, WS2_LINEAR, 7, 4, 1),
        ws::param("response", "Response", 0, 1, .4f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 8)};

    void prepare(const ws::Setup &s) {
        const double rate = s.sample_rate();
        const double top = std::min(18000.0, .45 * rate);
        const double ratio = top / 40.0;
        attack_alpha_ = float(std::exp(-1.0 / (.002 * rate)));
        for (uint32_t i = 0; i <= 256; ++i) {
            const double position = double(i) / 256.0;
            const double cutoff = 40.0 * std::pow(ratio, position);
            cutoff_g_[i] = float(std::tan(3.14159265358979323846 * cutoff / rate));
            const double release = .060 * std::pow(25.0, position);
            release_alpha_[i] = float(std::exp(-1.0 / (release * rate)));
        }
        reset();
    }
    void reset() {
        for (auto &channel : state_)
            for (auto &voice : channel) voice = {};
    }
    void process(ws::Block &b) {
        const uint32_t voices = std::min<uint32_t>(b.channels(), uint32_t(16));
        bool constant = true;
        for (uint32_t k = 0; k < 4; ++k) {
            constant = constant && b.in(k).constant() && b.in(k + 4).constant() && b.param(k).constant() &&
                       b.param(k + 4).constant();
        }
        constant = constant && b.param(8).constant();
        for (uint32_t k = 0; k < 4 && constant; ++k)
            for (uint32_t voice = 0; voice < voices; ++voice)
                if (b.param(k + 4).value(voice) >= .5f) constant = false;
        for (uint32_t k = 0; k < 5; ++k) {
            b.out(k).set_channels(voices);
            b.out(k).mark_constant(constant);
        }
        const uint32_t frames = constant ? 1u : b.frames();
        for (uint32_t voice = 0; voice < voices; ++voice) {
            std::array<ws::Channel, 4> input{};
            std::array<ws::Channel, 4> cv{};
            std::array<float *, 5> output{};
            for (uint32_t k = 0; k < 4; ++k) {
                // Normal the *input* of the preceding channel, recursively.
                input[k] = b.in(k).connected() ? b.in(k).ch(voice) : (k ? input[k - 1] : ws::Channel{});
                cv[k] = b.in(k + 4).connected() ? b.in(k + 4).ch(voice) : ws::Channel{};
                output[k] = b.out(k).ch(voice);
            }
            output[4] = b.out(4).ch(voice);
            for (uint32_t i = 0; i < frames; ++i) {
                float sum = 0;
                for (uint32_t k = 0; k < 4; ++k) {
                    const float level = b.param(k).value(voice, i);
                    const float cv_value = b.in(k + 4).connected() ? cv[k][i] : 1.f;
                    const float control = std::clamp(level * cv_value, 0.f, 1.f);
                    const float x = input[k][i];
                    float y;
                    auto &s = state_[k][voice];
                    if (b.param(k + 4).value(voice, i) < .5f) {
                        s = {}; // No stale LPG state after a VCA-only interval.
                        y = x * control;
                    } else {
                        const float response = std::clamp(b.param(8).value(voice, i), 0.f, 1.f);
                        const float decay = lookup(release_alpha_, response);
                        const float alpha = control > s.follower ? attack_alpha_ : decay;
                        s.follower = alpha * s.follower + (1.f - alpha) * control;
                        const float g = lookup(cutoff_g_, s.follower);
                        constexpr float k = 1.f / .6f;
                        const float a1 = 1.f / (1.f + g * (g + k));
                        const float v1 = (s.ic1 + g * (x - s.ic2)) * a1;
                        const float v2 = s.ic2 + g * v1;
                        s.ic1 = 2.f * v1 - s.ic1;
                        s.ic2 = 2.f * v2 - s.ic2;
                        y = v2 * s.follower;
                    }
                    output[k][i] = y;
                    sum += y;
                }
                output[4][i] = std::clamp(sum, -4.f, 4.f);
            }
        }
    }

private:
    struct VoiceState {
        float follower = 0, ic1 = 0, ic2 = 0;
    };
    static float lookup(const std::array<float, 257> &table, float x) {
        const float scaled = std::clamp(x, 0.f, 1.f) * 256.f;
        const uint32_t index = std::min<uint32_t>(uint32_t(scaled), uint32_t(255));
        return table[index] + (table[index + 1] - table[index]) * (scaled - float(index));
    }
    std::array<std::array<VoiceState, 16>, 4> state_{};
    std::array<float, 257> cutoff_g_{}, release_alpha_{};
    float attack_alpha_ = 0;
};
} // namespace quad_vca
