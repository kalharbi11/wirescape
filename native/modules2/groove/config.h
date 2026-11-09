// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace groove {
// Transient control-side byte payload; never serialized to a user patch.
enum Kind : uint8_t { Rest = 0, Note = 1, Tie = 2 };
struct Step {
    uint32_t kind = Rest;
    int32_t key = 60;
    float velocity = 0.f;
};
struct Config {
    uint32_t version = 1;
    float bpm = 120.f;
    uint32_t count = 8;
    std::array<Step, 8> steps{};
};
static_assert(sizeof(Step) == 12 && sizeof(Config) == 108, "bounded Groove config");
inline bool decode(const uint8_t *data, uint32_t bytes, Config &out) {
    Config next;
    if (bytes) {
        if (!data || bytes != sizeof(next)) return false;
        std::memcpy(&next, data, bytes);
    }
    if (next.version != 1 || !std::isfinite(next.bpm) || next.bpm < 20 || next.bpm > 300 || next.count < 1 ||
        next.count > 8)
        return false;
    for (const auto &s : next.steps)
        if (s.kind > Tie || s.key < 0 || s.key > 127 || !std::isfinite(s.velocity) || s.velocity < 0 || s.velocity > 1)
            return false;
    out = next;
    return true;
}
} // namespace groove
