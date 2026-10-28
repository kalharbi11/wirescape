// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include "comb_body.h"
#include "coupler.h"
#include "drift.h"
namespace chaos_lite {
struct ChaosLite {
    static constexpr ws::Info info{"essentials.chaos_lite", "CHAOS-lite", 1};
    static constexpr std::array inputs{
        ws::port("pitch", "Pitch", WS2_PITCH), ws::port("gate", "Gate", WS2_GATE, WS2_POLY, 1),
        ws::port("velocity", "Velocity", WS2_CV_UNI, WS2_POLY, 1), ws::port("fm", "FM", WS2_CV_BI)};
    static constexpr std::array outputs{
        ws::port("left", "Left", WS2_AUDIO, WS2_POLY, 0, "stereo"),
        ws::port("right", "Right", WS2_AUDIO, WS2_POLY, 0, "stereo")};
    static constexpr uint32_t stepped = WS2_STEPPED | WS2_AUTOMATABLE;
    static constexpr std::array params{
        ws::param("shape_a", "A shape", 0, 2, .4f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 0), ws::param("shape_b", "B shape", 0, 2, 1.6f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 1, 4, 0),
        ws::param("detune", "B detune", -60, 60, 7, "cent", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 2, 4, 0),
        ws::param("octave_a", "A octave", -3, 3, 0, "oct", stepped, WS2_LINEAR, 3, 4, 1),
        ws::param("octave_b", "B octave", -3, 3, -1, "oct", stepped, WS2_LINEAR, 4, 4, 1),
        ws::param("level_a", "A level", 0, 1, .55f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 5, 4, 0), ws::param("level_b", "B level", 0, 1, .35f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 6, 4, 0),
        ws::param("pan_a", "A pan", -1, 1, -.3f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 7, 4, 0), ws::param("pan_b", "B pan", -1, 1, .3f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 8, 4, 0),
        ws::param("route", "Route", 0, 2, 0, "", stepped, WS2_LINEAR, 9, 4, 1),
        ws::param("depth", "Coupling", 0, 1, .5f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 10, 4, 0), ws::param("body_tune", "Body tune", -2, 2, 0, "oct", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 11, 4, 0),
        ws::param("feedback", "Body feedback", 0, .92f, .3f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 12, 4, 0), ws::param("damping", "Body damping", 0, .99f, .4f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 13, 4, 0),
        ws::param("body_mix", "Body mix", 0, 1, .25f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 14, 4, 0), ws::param("roughness", "Roughness", 0, 1, .1f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 15, 4, 0),
        ws::param("rough_rate", "Roughness rate", 1, 150, 70, "Hz", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 16, 4, 0),
        ws::param("drift", "Drift", 0, 1, .1f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 17, 4, 0), ws::param("jitter", "Jitter", 0, 1, .02f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 18, 4, 0),
        ws::param("fall", "Fall", 0, 1, 0, "oct/s", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 19, 4, 0), ws::param("lfo_rate", "LFO rate", .02f, 12, .3f, "Hz", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 20, 4, 0),
        ws::param("lfo_depth", "LFO depth", 0, 1, .1f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 21, 4, 0),
        ws::param("lfo_target", "LFO target", 0, 2, 0, "", stepped, WS2_LINEAR, 22, 4, 1),
        ws::param("lfo_self", "LFO self", 0, 1, .2f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 23, 4, 0), ws::param("cutoff", "Cutoff", 20, 20000, 9000, "Hz", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 24, 4, 0),
        ws::param("level", "Output level", 0, 1, .5f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, 25, 4, 0)};
    struct Voice {
        CombBody body[2]; Coupler coupler[2]; Drift drift;
        float phase_a = 0, phase_b = 0, lfo = 0, lfo_previous = 0;
        float rough = 0, rough_phase = 0, filter[2]{}, dc_input[2]{}, dc_output[2]{};
    };
    std::array<Voice, 8> voices{};
    float rate = 48000;
    uint32_t capacity = 1, comb_size = 0, delay_size = 0;
    uint64_t seed = 0;
    static uint32_t comb_length(double rate) { return static_cast<uint32_t>(std::ceil(rate / 16.0)) + 4; }
    static uint32_t delay_length(double rate) { return static_cast<uint32_t>(std::ceil(rate * .006)) + 4; }
    static ws2_memory memory_requirements(const ws2_config& c) {
        ws2_memory m{}; m.struct_size = sizeof(m);
        m.alignment = 32;
        m.large_bytes = size_t(std::min(c.max_channels, 8u)) * 2 * ((3 * comb_length(c.sample_rate) + delay_length(c.sample_rate)) * sizeof(float) + 64);
        return m;
    }
    bool prepare(const ws::Setup& s) {
        rate = static_cast<float>(s.sample_rate()); capacity = std::min(s.max_channels(), 8u); seed = s.config.seed;
        comb_size = comb_length(rate); delay_size = delay_length(rate);
        for (uint32_t c = 0; c < capacity; ++c) for (uint32_t side = 0; side < 2; ++side) {
            auto* comb = static_cast<float*>(s.alloc(3 * comb_size * sizeof(float), 32, WS2_LARGE));
            auto* delay = static_cast<float*>(s.alloc(delay_size * sizeof(float), 32, WS2_LARGE));
            if (!comb || !delay) return false;
            voices[c].body[side].attach(comb, comb_size); voices[c].coupler[side].attach(delay, delay_size);
        }
        return true;
    }
    void reset() {
        for (uint32_t c = 0; c < capacity; ++c) {
            auto& v = voices[c];
            for (auto& body : v.body) body.reset(); for (auto& coupler : v.coupler) coupler.reset();
            v.phase_a = v.phase_b = v.lfo = v.lfo_previous = v.rough = v.rough_phase = 0;
            std::fill_n(v.filter, 2, 0.f); std::fill_n(v.dc_input, 2, 0.f); std::fill_n(v.dc_output, 2, 0.f);
            v.drift.reset(seed, c);
        }
    }
    static float wave(float phase, float shape) {
        const float saw = phase * 2.f - 1.f, square = phase < .5f ? 1.f : -1.f;
        if (shape < 1.f) return saw + shape * (square - saw);
        return square + (shape - 1.f) * (std::sin(6.28318530718f * phase) - square);
    }
    static void advance(float& phase, float step) { phase += step; phase -= std::floor(phase); }
    static float bound(float x, float lo, float hi) { return std::clamp(x, lo, hi); }
    void process(ws::Block& b) {
        const uint32_t n = b.channels();
        for (uint32_t side = 0; side < 2; ++side) { b.out(side).set_channels(n); b.out(side).mark_constant(false); }
        for (uint32_t c = 0; c < n; ++c) {
            if (c >= capacity) { for (uint32_t side = 0; side < 2; ++side) std::fill_n(b.out(side).ch(c), b.frames(), 0.f); continue; }
            auto& v = voices[c];
            for (uint32_t i = 0; i < b.frames(); ++i) {
                auto p = [&](uint32_t slot) { return b.param(slot).value(c, i); };
                const float gate = bound(b.in(1).value(c, i), 0, 1), velocity = bound(b.in(2).value(c, i), 0, 1);
                const float instability = v.drift.process(gate, p(17), p(18), p(19), rate);
                const float lfo = std::sin(6.28318530718f * v.lfo);
                advance(v.lfo, p(20) * std::max(.05f, 1.f + p(23) * v.lfo_previous) / rate);
                v.lfo_previous = lfo;
                const uint32_t target = static_cast<uint32_t>(std::clamp(std::lround(p(22)), 0l, 2l));
                const float mod = lfo * p(21);
                const float pitch = bound(b.in(0).value(c, i) + .25f * b.in(3).value(c, i) + instability, -4, 6);
                const float fa = bound(261.625565f * std::exp2(bound(pitch + std::round(p(3)) + (target == 0 ? mod : 0), -4, 8)), 16, rate * .45f);
                const float fb = bound(261.625565f * std::exp2(bound(pitch + std::round(p(4)) + p(2) / 1200.f, -4, 8)), 16, rate * .45f);
                const float a = wave(v.phase_a, p(0)) * p(5), bb = wave(v.phase_b, p(1)) * p(6);
                advance(v.phase_a, fa / rate); advance(v.phase_b, fb / rate);
                advance(v.rough_phase, p(16) / rate);
                if (v.rough_phase < p(16) / rate) v.rough = v.drift.random.bipolar();
                const float rough_gain = 1.f - p(15) * (.5f + .5f * v.rough);
                const float tune = bound(261.625565f * std::exp2(bound(pitch + p(11) + (target == 1 ? mod : 0), -4, 8)), 16, rate * .45f);
                const float coefficient = 1.f - std::exp(-6.28318530718f * bound(p(24) * (target == 2 ? std::exp2(mod * 3) : 1), 20, rate * .45f) / rate);
                for (uint32_t side = 0; side < 2; ++side) {
                    const float sign = side == 0 ? -1.f : 1.f;
                    const float aa = a * (.5f + sign * p(7) * .5f), bpan = bb * (.5f + sign * p(8) * .5f);
                    const uint32_t mode = static_cast<uint32_t>(std::clamp(std::lround(p(9)), 0l, 2l));
                    const float coupled = v.coupler[side].process(aa, bpan, mode, p(10), rate);
                    const float body = v.body[side].process(coupled, rate / tune, p(12), p(13));
                    const float x = (coupled + p(14) * (body - coupled)) * rough_gain * gate * velocity;
                    v.filter[side] += coefficient * (x - v.filter[side]);
                    const float dc = v.filter[side] - v.dc_input[side] + .995f * v.dc_output[side];
                    v.dc_input[side] = v.filter[side]; v.dc_output[side] = dc;
                    const float y = dc * p(25);
                    b.out(side).ch(c)[i] = bound(y / (1.f + std::abs(y)), -.8912509f, .8912509f);
                }
            }
        }
    }
    // Patch-state policy: instability state survives; delay/comb/filter history resets.
    int save_state(ws::Writer w) {
        if (!w.u32(1)) return WS2_CAPACITY;
        for (const auto& v : voices) {
            const auto rng = v.drift.random.save();
            for (auto word : {uint32_t(rng.state), uint32_t(rng.state >> 32), uint32_t(rng.sequence), uint32_t(rng.sequence >> 32)}) if (!w.u32(word)) return WS2_CAPACITY;
            for (float value : {v.phase_a, v.phase_b, v.lfo, v.lfo_previous, v.rough, v.rough_phase, v.drift.current, v.drift.target, v.drift.jitter, v.drift.fall}) if (!w.f32(value)) return WS2_CAPACITY;
            if (!w.u32(v.drift.tick) || !w.u32(v.drift.high ? 1u : 0u)) return WS2_CAPACITY;
        }
        return WS2_OK;
    }
    int load_state(ws::Reader r) {
        uint32_t version; if (!r.u32(version)) return WS2_INVALID; if (version != 1) return WS2_UNSUPPORTED;
        auto staged = voices;
        for (auto& v : staged) {
            uint32_t words[4], high;
            for (auto& word : words) if (!r.u32(word)) return WS2_INVALID;
            float* values[] = {&v.phase_a, &v.phase_b, &v.lfo, &v.lfo_previous, &v.rough, &v.rough_phase, &v.drift.current, &v.drift.target, &v.drift.jitter, &v.drift.fall};
            for (auto value : values) if (!r.f32(*value) || !std::isfinite(*value)) return WS2_INVALID;
            if (!r.u32(v.drift.tick) || v.drift.tick >= 128 || !r.u32(high) || high > 1) return WS2_INVALID;
            if (v.phase_a < 0 || v.phase_a >= 1 || v.phase_b < 0 || v.phase_b >= 1 || v.lfo < 0 || v.lfo >= 1 || v.rough_phase < 0 || v.rough_phase >= 1 || std::abs(v.lfo_previous) > 1 || std::abs(v.rough) > 1 || std::abs(v.drift.current) > 1 || std::abs(v.drift.target) > 1 || std::abs(v.drift.jitter) > 1 || v.drift.fall < 0 || v.drift.fall > 4 || !(words[2] & 1)) return WS2_INVALID;
            v.drift.random.restore({uint64_t(words[0]) | uint64_t(words[1]) << 32, uint64_t(words[2]) | uint64_t(words[3]) << 32}); v.drift.high = high != 0;
        }
        reset();
        for (uint32_t c = 0; c < capacity; ++c) {
            const auto& s = staged[c]; auto& v = voices[c];
            v.drift = s.drift; v.phase_a = s.phase_a; v.phase_b = s.phase_b; v.lfo = s.lfo; v.lfo_previous = s.lfo_previous; v.rough = s.rough; v.rough_phase = s.rough_phase;
        }
        return WS2_OK;
    }
};
}
