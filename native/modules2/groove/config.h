// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace groove {
// Transient control-side POD only. Authored patch persistence remains JSON.
// Four tracks x 64 steps reserved; at most two tracks are active in v2.
enum Kind : uint8_t { Rest = 0, Note = 1, Tie = 2 };
enum Condition : uint8_t { Always = 0, Chance = 1, AB = 2, First = 3, NotFirst = 4 };
struct Step {
    uint32_t kind = Rest;
    int32_t key = 60;
    float velocity = 0.f;
    float mod1 = 0.f;
    float mod2 = 0.f;
    uint8_t gate_q = 4; // Quarter-step units 1,2,3,4; default full-step.
    uint8_t cond_type = Always;
    uint8_t cond_a = 0; // Chance percent or A in A:B.
    uint8_t cond_b = 0; // B in A:B.
    uint8_t key_count = 1; // One note when the authored step omits "keys".
    std::array<int32_t, 5> keys{{60, 0, 0, 0, 0}};
};
struct Track {
    uint32_t armed = 1;
    uint32_t mute = 0;
    uint32_t length = 32; // Legacy default; the v2 JSON adapter defaults new tracks to 16.
    uint32_t div = 1;     // Sixteenth ticks per track step (1, 2, 4 or 8).
    std::array<Step, 64> steps{};
};
struct Config {
    uint32_t version = 2;
    float bpm = 120.f;
    uint32_t track_count = 1;
    uint32_t instance_seed = 0; // Off-render graph numeric ID, never authored JSON.
    uint32_t authored_version = 1; // Preserve old 32-step entry timing for v1; v2 enters at bar 16.
    std::array<Track, 4> tracks{};
};
static_assert(sizeof(Step) == 48 && sizeof(Track) == 3088 && sizeof(Config) == 12372,
              "Groove v2 fixed 4x64 POD layout requires contract review");

// The previous transient v1 layout is read only for compatibility with direct
// legacy callers; no legacy bytes are written back to an authored patch.
struct LegacyStep {
    uint32_t kind = Rest;
    int32_t key = 60;
    float velocity = 0.f;
    float mod1 = 0.f;
    float mod2 = 0.f;
};
struct LegacyTrack {
    uint32_t armed = 1;
    uint32_t mute = 0;
    std::array<LegacyStep, 64> steps{};
};
struct LegacyConfig {
    uint32_t version = 1;
    float bpm = 120.f;
    uint32_t track_count = 1;
    std::array<LegacyTrack, 4> tracks{};
};
static_assert(sizeof(LegacyStep) == 20 && sizeof(LegacyTrack) == 1288 && sizeof(LegacyConfig) == 5164,
              "Legacy Groove layout must stay byte-identical");

inline bool valid_step(const Step &s) {
    if (s.kind > Tie || s.key < 0 || s.key > 127 ||
        !std::isfinite(s.velocity) || s.velocity < 0 || s.velocity > 1 ||
        !std::isfinite(s.mod1) || s.mod1 < 0 || s.mod1 > 1 ||
        !std::isfinite(s.mod2) || s.mod2 < 0 || s.mod2 > 1 ||
        s.gate_q < 1 || s.gate_q > 4 || s.cond_type > NotFirst) return false;
    if (s.kind != Note && (s.cond_type != Always || s.key_count != 1)) return false;
    if (s.key_count < 1 || s.key_count > 5 || s.keys[0] != s.key) return false;
    for (uint32_t i = 1; i < s.key_count; ++i) {
        if (s.keys[i] < 0 || s.keys[i] > 127 || s.keys[i] == s.key || (i > 1 && s.keys[i] <= s.keys[i - 1]))
            return false;
    }
    switch (s.cond_type) {
    case Always: case First: case NotFirst: return s.cond_a == 0 && s.cond_b == 0;
    case Chance: return s.cond_a >= 1 && s.cond_a <= 99 && s.cond_b == 0;
    case AB: return s.cond_b >= 2 && s.cond_b <= 8 && s.cond_a >= 1 && s.cond_a <= s.cond_b;
    default: return false;
    }
}

inline bool decode(const uint8_t *data, uint32_t bytes, Config &out) {
    Config next;
    if (bytes == sizeof(LegacyConfig)) {
        if (!data) return false;
        LegacyConfig old;
        std::memcpy(&old, data, bytes);
        if (old.version != 1) return false;
        next.bpm = old.bpm;
        next.track_count = old.track_count;
        for (size_t t = 0; t < next.tracks.size(); ++t) {
            next.tracks[t].armed = old.tracks[t].armed;
            next.tracks[t].mute = old.tracks[t].mute;
            for (size_t i = 0; i < next.tracks[t].steps.size(); ++i) {
                const auto &from = old.tracks[t].steps[i];
                auto &to = next.tracks[t].steps[i];
                to.kind = from.kind;
                to.key = from.key;
                to.keys[0] = from.key;
                to.velocity = from.velocity;
                to.mod1 = from.mod1;
                to.mod2 = from.mod2;
            }
        }
    } else if (bytes) {
        if (!data || bytes != sizeof(Config)) return false;
        std::memcpy(&next, data, bytes);
    }
    if (next.version != 2 || !std::isfinite(next.bpm) || next.bpm < 20 || next.bpm > 300 ||
        next.track_count < 1 || next.track_count > 2 ||
        (next.authored_version != 1 && next.authored_version != 2)) return false;
    for (const auto &track : next.tracks) {
        if (track.armed > 1 || track.mute > 1 ||
            (track.length != 8 && track.length != 16 && track.length != 32 && track.length != 64) ||
            (track.div != 1 && track.div != 2 && track.div != 4 && track.div != 8))
            return false;
        for (const auto &step : track.steps)
            if (!valid_step(step)) return false;
    }
    out = next;
    return true;
}
} // namespace groove
