// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace resonator {
// Adapted from our own native/modules2/chaos_lite/comb_body.h.
// Three independent fractional-delay combs with loop saturation and one-pole damping.
struct CombBody {
    float *data = nullptr;
    uint32_t length = 0, cursor = 0;
    float low[3]{};

    void attach(float *memory, uint32_t n) {
        data = memory;
        length = n;
    }
    void reset() {
        if (data) std::fill_n(data, length * 3, 0.f);
        cursor = 0;
        std::fill_n(low, 3, 0.f);
    }
    struct Loop {
        const float (&delay)[3];
        const float (&gain)[3];
    };
    float process(float input, Loop loop, float damping) {
        constexpr float weights[3]{.6f, .25f, .15f};
        float sum = 0.f;
        for (uint32_t tap = 0; tap < 3; ++tap) {
            const float d = std::clamp(loop.delay[tap], 2.f, float(length - 2));
            const uint32_t whole = static_cast<uint32_t>(d);
            const float fraction = d - float(whole);
            const uint32_t at = (cursor + length - whole) % length;
            const uint32_t previous = (at + length - 1) % length;
            float *row = data + size_t(tap) * length;
            const float delayed = row[at] + fraction * (row[previous] - row[at]);
            low[tap] = delayed * (1.f - damping) + low[tap] * damping;
            const float x = input + low[tap] * loop.gain[tap];
            row[cursor] = x / (1.f + std::abs(x));
            sum += weights[tap] * delayed;
        }
        cursor = (cursor + 1) % length;
        return sum;
    }
};
} // namespace resonator
