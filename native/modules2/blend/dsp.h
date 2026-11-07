// SPDX-License-Identifier: GPL-3.0-only
// sound.blend: equal-power crossfade and splice between two sources. Concept from the owner's
// Conjunctions prototype (mix, splice); original code. See DESIGN.md.
#pragma once
#include "wirescape_sdk.h"

namespace blend {

enum Mode : uint8_t { Crossfade, Splice, kModes };
enum Param : uint8_t { P_MODE, P_BALANCE, P_SEG_A, P_SEG_B, P_FADE, P_JITTER, kParams };
constexpr uint32_t kStateVersion = 1;
constexpr uint32_t kStepMod = WS2_STEPPED | WS2_MODULATABLE | WS2_AUTOMATABLE, kMod = WS2_MODULATABLE | WS2_AUTOMATABLE;
constexpr float kHalfPi = 1.57079632679489661923f, kPi = 3.14159265358979323846f;

struct Blend {
    static constexpr ws::Info info{"sound.blend", "Blend", 1};
    static constexpr std::array inputs{ws::port("a", "A", WS2_AUDIO, WS2_SUM), ws::port("b", "B", WS2_AUDIO, WS2_SUM),
                                       ws::port("switch", "Switch", WS2_TRIGGER, WS2_FIRST)};
    static constexpr std::array outputs{ws::port("audio", "Audio", WS2_AUDIO)};
    static constexpr std::array params{
        ws::param("mode", "Mode", 0, kModes - 1, Crossfade, "", kStepMod, WS2_LINEAR, P_MODE, 4, 1),
        ws::param("balance", "Balance", 0, 1, .5f, "", kMod, WS2_LINEAR, P_BALANCE),
        ws::param("seg_a", "A segment", .01f, 2, .22f, "s", kMod, WS2_LOG, P_SEG_A),
        ws::param("seg_b", "B segment", .01f, 2, .22f, "s", kMod, WS2_LOG, P_SEG_B),
        ws::param("fade", "Fade", .001f, .2f, .01f, "s", kMod, WS2_LOG, P_FADE),
        ws::param("jitter", "Jitter", 0, 1, .3f, "", kMod, WS2_LINEAR, P_JITTER)};

    void prepare(const ws::Setup &s) {
        rate_ = s.sample_rate();
        seed_ = s.config.seed;
        reset();
    }
    void reset() {
        position_ = .5f;
        glide_from_ = .5f;
        glide_at_ = glide_len_ = 0;
        mode_ = Crossfade;
        source_ = 0;
        remaining_ = 0;
        flipped_ = false;
        switch_high_ = false;
        rng_.seed(seed_);
    }

    void process(ws::Block &b) {
        auto out = b.out(0);
        out.set_channels(1);
        float *y = out.ch(0);
        const ws::Channel a = b.in(0).ch(0), bb = b.in(1).ch(0), sw = b.in(2).ch(0);
        // Nothing moves when every input is constant, the mode stays Crossfade and no glide runs:
        // only sample 0 can carry a trigger edge (which would start a glide).
        const bool still = b.in(0).constant() && b.in(1).constant() && b.in(2).constant() &&
                           b.param(P_MODE).constant() && b.param(P_BALANCE).constant() && b.param(P_FADE).constant();
        const uint32_t frames = b.frames();
        for (uint32_t i = 0; i < frames; ++i) {
            if (still && i == 1 && mode_ == Crossfade && glide_len_ == 0) {
                out.mark_constant(true);
                return;
            }
            y[i] = step(b, i, {a[i], bb[i], sw[i]});
        }
        out.mark_constant(false);
    }

    int save_state(ws::Writer w) {
        const ws::Random::State r = rng_.save();
        uint32_t position, from;
        std::memcpy(&position, &position_, 4);
        std::memcpy(&from, &glide_from_, 4);
        const uint32_t words[] = {kStateVersion,
                                  position,
                                  from,
                                  glide_at_,
                                  glide_len_,
                                  mode_,
                                  source_,
                                  remaining_,
                                  uint32_t(flipped_) | uint32_t(switch_high_) << 1,
                                  uint32_t(r.state),
                                  uint32_t(r.state >> 32),
                                  uint32_t(r.sequence),
                                  uint32_t(r.sequence >> 32)};
        for (uint32_t v : words)
            if (!w.u32(v)) return WS2_CAPACITY;
        return WS2_OK;
    }
    int load_state(ws::Reader r) {
        uint32_t version;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != kStateVersion) return WS2_UNSUPPORTED;
        uint32_t v[12];
        for (uint32_t &x : v)
            if (!r.u32(x)) return WS2_INVALID;
        float position, from;
        std::memcpy(&position, &v[0], 4);
        std::memcpy(&from, &v[1], 4);
        const uint32_t limit = uint32_t(rate_ * 8); // longest segment: 2 s x (1 + jitter), any rate here
        if (!(position >= 0 && position <= 1) || !(from >= 0 && from <= 1) || v[3] > limit || v[2] > v[3] ||
            v[4] >= kModes || v[5] > 1 || v[6] > limit || v[7] > 3 || !(v[10] & 1u))
            return WS2_INVALID;
        position_ = position;
        glide_from_ = from;
        glide_at_ = v[2];
        glide_len_ = v[3];
        mode_ = v[4];
        source_ = v[5];
        remaining_ = v[6];
        flipped_ = v[7] & 1u;
        switch_high_ = v[7] & 2u;
        rng_.restore({uint64_t(v[8]) | uint64_t(v[9]) << 32, uint64_t(v[10]) | uint64_t(v[11]) << 32});
        return WS2_OK;
    }

  private:
    uint32_t samples(float seconds) const { return uint32_t(std::max(1.0, std::round(double(seconds) * rate_))); }
    void start_glide(uint32_t length) {
        glide_from_ = position_;
        glide_at_ = 0;
        glide_len_ = length;
    }
    // A new segment for source_: its length is read now, varied by +-jitter, at least two fades long.
    void begin_segment(const ws::Block &b, uint32_t i) {
        const float seconds = b.param(source_ ? P_SEG_B : P_SEG_A).value(0, i);
        const float jitter = std::clamp(b.param(P_JITTER).value(0, i), 0.f, 1.f);
        const double length = double(std::clamp(seconds, .01f, 2.f)) * rate_ * (1.0 + jitter * rng_.bipolar());
        remaining_ = uint32_t(std::max(double(2 * fade_), std::round(length)));
    }
    struct Inputs {
        float a, b, sw;
    };
    float step(const ws::Block &b, uint32_t i, Inputs x) {
        const uint32_t mode = uint32_t(std::clamp<long>(std::lround(b.param(P_MODE).value(0, i)), 0, kModes - 1));
        fade_ = samples(std::clamp(b.param(P_FADE).value(0, i), .001f, .2f));
        const uint32_t fade = fade_;
        const bool high = x.sw > .5f ? true : x.sw < .25f ? false : switch_high_; // Schmitt, mirrored for state
        const bool edge = high && !switch_high_;
        switch_high_ = high;
        if (mode != mode_) {
            mode_ = mode;
            start_glide(fade);
            if (mode == Splice) {
                source_ = position_ >= .5f ? 1u : 0u;
                begin_segment(b, i);
            }
        }
        float target;
        if (mode_ == Splice) {
            if (edge || remaining_ == 0) {
                source_ ^= 1u;
                begin_segment(b, i);
                start_glide(fade);
            }
            --remaining_;
            target = float(source_);
        } else {
            if (edge) {
                flipped_ = !flipped_;
                start_glide(fade);
            }
            const float balance = std::clamp(b.param(P_BALANCE).value(0, i), 0.f, 1.f);
            target = flipped_ ? 1.f - balance : balance;
        }
        if (glide_len_) { // raised cosine from where the position was towards the moving target
            ++glide_at_;
            const float w = .5f - .5f * std::cos(kPi * float(glide_at_) / float(glide_len_));
            position_ = glide_from_ + (target - glide_from_) * w;
            if (glide_at_ >= glide_len_) glide_len_ = glide_at_ = 0;
        } else position_ = target;
        return x.a * std::cos(kHalfPi * position_) + x.b * std::sin(kHalfPi * position_);
    }

    double rate_ = 48000;
    uint64_t seed_ = 0;
    float position_ = .5f, glide_from_ = .5f;
    uint32_t fade_ = 1, glide_at_ = 0, glide_len_ = 0, mode_ = Crossfade, source_ = 0, remaining_ = 0;
    bool flipped_ = false, switch_high_ = false;
    ws::Random rng_;
};

} // namespace blend
