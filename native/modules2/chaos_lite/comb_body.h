// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace chaos_lite {
// Three independently damped combs; storage is provided once by prepare().
struct CombBody {
    float* data = nullptr;
    uint32_t length = 0, cursor = 0;
    float low[3]{};
    void attach(float* memory, uint32_t n) { data = memory; length = n; }
    void reset() {
        if (data) std::fill_n(data, length * 3, 0.f);
        cursor = 0;
        std::fill_n(low, 3, 0.f);
    }
    float process(float input, float delay, float feedback, float damping) {
        constexpr float ratios[] = {1.f, 1.4983f, 2.003f};
        float sum = 0;
        for (uint32_t tap = 0; tap < 3; ++tap) {
            const float d = std::clamp(delay / ratios[tap], 2.f, float(length - 2));
            const uint32_t whole = static_cast<uint32_t>(d);
            const float fraction = d - whole;
            const uint32_t at = (cursor + length - whole) % length;
            const uint32_t previous = (at + length - 1) % length;
            float* row = data + tap * length;
            const float delayed = row[at] + fraction * (row[previous] - row[at]);
            low[tap] = delayed * (1.f - damping) + low[tap] * damping;
            const float x = input + low[tap] * feedback;
            row[cursor] = x / (1.f + std::abs(x));
            sum += delayed;
        }
        cursor = (cursor + 1) % length;
        return sum / 3.f;
    }
};
}
