// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace chaos_lite {
// A/B coupling is independent of the resonator, ready to lift into an M7 module.
struct Coupler {
    float* delay = nullptr;
    uint32_t length = 0, cursor = 0;
    void attach(float* memory, uint32_t n) { delay = memory; length = n; }
    void reset() { if (delay) std::fill_n(delay, length, 0.f); cursor = 0; }
    float process(float a, float b, uint32_t mode, float depth, float rate) {
        delay[cursor] = a;
        float coupled = a;
        if (mode == 1) coupled = a * b;
        if (mode == 2) {
            const float distance = rate * (.0035f + .0025f * std::clamp(b, -1.f, 1.f));
            const float d = std::clamp(distance, 1.f, float(length - 2));
            const uint32_t whole = static_cast<uint32_t>(d);
            const uint32_t at = (cursor + length - whole) % length;
            const uint32_t prior = (at + length - 1) % length;
            coupled = delay[at] + (d - whole) * (delay[prior] - delay[at]);
        }
        cursor = (cursor + 1) % length;
        return (a + depth * (coupled - a)) + b;
    }
};
}
