// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace groove {
// Transient control-side POD only: patch persistence always uses versioned JSON.
// Reserve four tracks of 64 steps; S1 activates at most two tracks of 32.
enum Kind : uint8_t { Rest = 0, Note = 1, Tie = 2 };
struct Step {
    uint32_t kind = Rest;
    int32_t key = 60;
    float velocity = 0.f;
    float mod1 = 0.f;
    float mod2 = 0.f;
};
struct Track {
    uint32_t armed = 1;
    uint32_t mute = 0;
    std::array<Step, 64> steps{};
};
struct Config {
    uint32_t version = 1;
    float bpm = 120.f;
    uint32_t track_count = 1;
    std::array<Track, 4> tracks{};
};
static_assert(sizeof(Step) == 20 && sizeof(Track) == 1288 && sizeof(Config) == 5164,
              "Groove fixed 4x64 POD layout must be reviewed before changing");
inline bool decode(const uint8_t *data, uint32_t bytes, Config &out) {
    Config next;
    if (bytes) {
        if (!data || bytes != sizeof(next)) return false;
        std::memcpy(&next, data, bytes);
    }
    if (next.version != 1 || !std::isfinite(next.bpm) || next.bpm < 20 || next.bpm > 300 ||
        next.track_count < 1 || next.track_count > 2)
        return false;
    for (const auto &t : next.tracks) {
        if (t.armed > 1 || t.mute > 1) return false;
        for (const auto &s : t.steps)
            if (s.kind > Tie || s.key < 0 || s.key > 127 || !std::isfinite(s.velocity) ||
                s.velocity < 0 || s.velocity > 1 || !std::isfinite(s.mod1) || s.mod1 < 0 ||
                s.mod1 > 1 || !std::isfinite(s.mod2) || s.mod2 < 0 || s.mod2 > 1)
                return false;
    }
    out = next;
    return true;
}
} // namespace groove
