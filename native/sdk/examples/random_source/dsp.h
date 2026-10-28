// SPDX-License-Identifier: MIT
// Example.random_source: clocked sample & hold with a smoothed twin output.
// Teaching module: one statement per line where it helps, a comment at every rule.
#pragma once
#include "wirescape_sdk.h"

struct RandomSource {
    static constexpr ws::Info info{"example.random_source", "Random Source", 1};

    // Mono clock fans out to every voice: the host resolves broadcast before process().
    static constexpr std::array inputs{
        ws::port("clock", "Clock", WS2_TRIGGER, WS2_POLY)};
    static constexpr std::array outputs{
        ws::port("step", "Step", WS2_CV_BI),
        ws::port("smooth", "Smooth", WS2_CV_BI)};

    // Parameter IDs are API: never rename or reorder once published.
    // "voices" is STEPPED (no host smoothing) and not MODULATABLE: it picks the output channel count.
    static constexpr std::array params{
        ws::param("voices", "Voices", 1, 16, 1, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("smooth", "Smooth", 0, 1, 0.05f, "s"),
        ws::param("range", "Range", 0, 1, 1)};

    static constexpr uint32_t kMaxVoices = 16;
    // Below this distance the smoothed value snaps to the held value. It lets the output become
    // CONSTANT (a one-pole never reaches its target exactly) and is far below audibility.
    static constexpr double kEpsilon = 1.0e-6;

    struct Voice {
        ws::Random rng;
        ws::Schmitt schmitt;
        bool high = false;   // Schmitt output last sample; Schmitt has no getter, so we mirror it.
        float held = 0;      // sample & hold value in [-1, 1), before the range scale
        double smooth = 0;   // double on purpose: a float one-pole stalls ~1e-3 short of its target
    };                       // when the time constant is long (increment below half an ulp).

    std::array<Voice, kMaxVoices> voices{};
    uint64_t seed = 0;
    double rate = 48000;
    uint32_t max_channels = 1;

    void prepare(const ws::Setup& s) {
        seed = s.config.seed;           // host-provided, so one patch seed makes a repeatable patch
        rate = s.sample_rate();
        max_channels = s.max_channels();
    }

    // Deterministic: the same seed always restarts the same sequences.
    void reset() {
        for (uint32_t c = 0; c < kMaxVoices; ++c) {
            voices[c] = Voice{};
            voices[c].rng.seed(seed, c);   // channel index decorrelates the voices
        }
    }

    // One-pole coefficient for a time constant in seconds; 0 means "no smoothing".
    double coefficient(float seconds) const noexcept {
        return seconds > 0 ? 1.0 - std::exp(-1.0 / (static_cast<double>(seconds) * rate)) : 1.0;
    }

    // Advance the S&H for one clock sample.
    static void clock_sample(Voice& v, float clock) noexcept {
        const bool now = v.schmitt.process(clock);
        if (now && !v.high) v.held = v.rng.bipolar();   // rising edge: fresh value
        v.high = now;
    }

    // Advance the smoothed value one sample and snap when converged.
    static void smooth_sample(Voice& v, double k) noexcept {
        v.smooth += k * (static_cast<double>(v.held) - v.smooth);
        if (std::abs(static_cast<double>(v.held) - v.smooth) < kEpsilon) v.smooth = v.held;
    }

    void process(ws::Block& b) {
        const uint32_t frames = b.frames();
        // The source decides the channel count from its parameter (read once per block).
        const long wanted = std::lround(b.param(0).value(0, 0));
        const uint32_t n = static_cast<uint32_t>(std::clamp<long>(wanted, 1, std::min(kMaxVoices, max_channels)));
        b.out(0).set_channels(n);
        b.out(1).set_channels(n);

        const bool clock_constant = b.in(0).constant();
        const bool range_constant = b.param(2).constant();

        if (clock_constant) {
            // CONSTANT fast path: a constant clock can only produce an edge at sample 0,
            // so handle that one sample, then the held value cannot change this block.
            for (uint32_t c = 0; c < n; ++c) clock_sample(voices[c], b.in(0).ch(c)[0]);

            // step is CONSTANT whenever held values and range are.
            b.out(0).mark_constant(range_constant);
            for (uint32_t c = 0; c < n; ++c) {
                const auto range = b.param(2).ch(c);
                float* step = b.out(0).ch(c);
                for (uint32_t i = 0; i < (range_constant ? 1u : frames); ++i) step[i] = voices[c].held * range[i];
            }

            // smooth is CONSTANT only if every voice already sits on its held value.
            bool converged = range_constant;
            for (uint32_t c = 0; c < n; ++c)
                converged = converged && std::abs(static_cast<double>(voices[c].held) - voices[c].smooth) < kEpsilon;
            b.out(1).mark_constant(converged);
            for (uint32_t c = 0; c < n; ++c) {
                Voice& v = voices[c];
                const auto range = b.param(2).ch(c);
                const auto time = b.param(1).ch(c);
                float* smooth = b.out(1).ch(c);
                if (converged) {
                    v.smooth = v.held;   // snap: same result the full loop would reach this sample
                    smooth[0] = v.held * range[0];
                    continue;
                }
                const double k0 = coefficient(time[0]);
                for (uint32_t i = 0; i < frames; ++i) {
                    smooth_sample(v, time.is_constant ? k0 : coefficient(time[i]));
                    smooth[i] = static_cast<float>(v.smooth) * range[i];
                }
            }
            return;
        }

        // General path: clock varies, so every sample may carry an edge. Fill every sample.
        b.out(0).mark_constant(false);
        b.out(1).mark_constant(false);
        for (uint32_t c = 0; c < n; ++c) {
            Voice& v = voices[c];
            const auto clock = b.in(0).ch(c);
            const auto range = b.param(2).ch(c);
            const auto time = b.param(1).ch(c);
            float* step = b.out(0).ch(c);
            float* smooth = b.out(1).ch(c);
            const double k0 = coefficient(time[0]);
            for (uint32_t i = 0; i < frames; ++i) {
                clock_sample(v, clock[i]);
                smooth_sample(v, time.is_constant ? k0 : coefficient(time[i]));
                step[i] = v.held * range[i];
                smooth[i] = static_cast<float>(v.smooth) * range[i];
            }
        }
    }

    // State v1 (portable little-endian u32 only): per voice, 4x u32 Random, f32 held,
    // 2x u32 smooth (the double's bit pattern), u32 Schmitt-high flag.
    int save_state(ws::Writer w) {
        if (!w.u32(1)) return WS2_CAPACITY;
        for (const Voice& v : voices) {
            const ws::Random::State r = v.rng.save();
            uint64_t smooth_bits;
            std::memcpy(&smooth_bits, &v.smooth, sizeof(smooth_bits));
            const uint32_t words[] = {static_cast<uint32_t>(r.state), static_cast<uint32_t>(r.state >> 32),
                                      static_cast<uint32_t>(r.sequence), static_cast<uint32_t>(r.sequence >> 32)};
            for (uint32_t word : words) if (!w.u32(word)) return WS2_CAPACITY;
            if (!w.f32(v.held) || !w.u32(static_cast<uint32_t>(smooth_bits)) ||
                !w.u32(static_cast<uint32_t>(smooth_bits >> 32)) || !w.u32(v.high ? 1u : 0u)) return WS2_CAPACITY;
        }
        return WS2_OK;
    }

    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;   // newer state: host keeps the opaque bytes
        std::array<Voice, kMaxVoices> staged = voices;   // validate into a copy, commit at the end
        for (Voice& v : staged) {
            uint32_t w[4], lo, hi, flag;
            for (uint32_t& word : w) if (!r.u32(word)) return WS2_INVALID;
            if (!r.f32(v.held) || !r.u32(lo) || !r.u32(hi) || !r.u32(flag)) return WS2_INVALID;
            const uint64_t smooth_bits = (static_cast<uint64_t>(hi) << 32) | lo;
            std::memcpy(&v.smooth, &smooth_bits, sizeof(v.smooth));
            const ws::Random::State rs{(static_cast<uint64_t>(w[1]) << 32) | w[0], (static_cast<uint64_t>(w[3]) << 32) | w[2]};
            const bool sane = std::isfinite(v.held) && std::abs(v.held) <= 1.f && std::isfinite(v.smooth) &&
                              std::abs(v.smooth) <= 1.0 && flag <= 1 && (rs.sequence & 1) == 1;   // LCG stream is odd
            if (!sane) return WS2_INVALID;
            v.rng.restore(rs);
            v.high = flag != 0;
            v.schmitt.reset();
            if (v.high) v.schmitt.process(1.f);
        }
        voices = staged;
        return WS2_OK;
    }
};
