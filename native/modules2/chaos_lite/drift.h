// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
namespace chaos_lite {
// Seeded slow drift and fixed 128-frame jitter clock; independent of host block cuts.
struct Drift {
    ws::Random random;
    float current = 0, target = 0, jitter = 0, fall = 0;
    uint32_t tick = 0;
    bool high = false;
    void reset(uint64_t seed, uint32_t channel) {
        *this = Drift{};
        random.seed(seed, channel);
    }
    float process(float gate, float amount, float jitter_amount, float fall_amount, float rate) {
        const bool now = gate > (high ? .25f : .5f);
        if (now && !high) fall = 0;
        high = now;
        if (tick == 0) { target = random.bipolar(); jitter = random.bipolar(); }
        tick = (tick + 1) % 128;
        current += (target - current) * (2.f / rate);
        if (high) fall = std::min(4.f, fall + fall_amount / rate);
        return .04f * amount * current + .015f * jitter_amount * jitter - fall;
    }
};
}
