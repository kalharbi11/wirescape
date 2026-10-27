// SPDX-License-Identifier: MIT
#pragma once

#include "wirescape_sdk.h"

struct RingMod {
    static constexpr ws::Info info{"example.ringmod2", "Example Ring Mod", 1};
    static constexpr std::array inputs{
        ws::port("a", "A", WS2_AUDIO),
        ws::port("b", "B", WS2_AUDIO)
    };
    static constexpr std::array outputs{
        ws::port("audio", "Audio", WS2_AUDIO)
    };
    static constexpr std::array params{
        ws::param("level", "Level", 0.0f, 1.0f, 1.0f)
    };

    void prepare(const ws::Setup&) {}
    void reset() {}

    void process(ws::Block& b) {
        b.out(0).set_channels(b.channels());
        const bool constant = b.in(0).constant() && b.in(1).constant() && b.param(0).constant();
        b.out(0).mark_constant(constant);
        for (uint32_t c = 0; c < b.channels(); ++c) {
            const auto a = b.in(0).ch(c);
            const auto x = b.in(1).ch(c);
            const auto level = b.param(0).ch(c);
            auto output = b.out(0).ch(c);
            for (uint32_t i = 0; i < (constant ? 1u : b.frames()); ++i)
                output[i] = a[i] * x[i] * level[i];
        }
    }
};
