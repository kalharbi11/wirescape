// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "rng_scope.h"
#include "wirescape_sdk.h"
#include "plaits/dsp/voice.h"
#include <new>
namespace macro_oscillator {
struct MacroOscillator {
    static constexpr ws::Info info{"ports.macro_oscillator", "Macro Oscillator", 1};
    static constexpr std::array inputs{
        ws::port("pitch", "Pitch", WS2_PITCH), ws::port("model_cv", "Model CV", WS2_CV_BI),
        ws::port("fm", "FM", WS2_CV_BI), ws::port("harmonics_cv", "Harmonics", WS2_CV_BI),
        ws::port("timbre_cv", "Timbre", WS2_CV_BI), ws::port("morph_cv", "Morph", WS2_CV_BI),
        ws::port("trigger", "Trigger", WS2_GATE), ws::port("level", "Level", WS2_CV_UNI)};
    static constexpr std::array outputs{ws::port("main", "Main", WS2_AUDIO), ws::port("aux", "Aux", WS2_AUDIO)};
    static constexpr uint32_t flags = WS2_MODULATABLE | WS2_AUTOMATABLE | WS2_PER_VOICE;
    static constexpr std::array params{
        ws::param("model", "Model", 0, 23, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1),
        ws::param("coarse", "Coarse", -48, 48, 0, "st", flags, WS2_LINEAR, 1, 4, 0),
        ws::param("harmonics", "Harmonics", 0, 1, .5f, "", flags, WS2_LINEAR, 2, 4, 0),
        ws::param("timbre", "Timbre", 0, 1, .5f, "", flags, WS2_LINEAR, 3, 4, 0),
        ws::param("morph", "Morph", 0, 1, .5f, "", flags, WS2_LINEAR, 4, 4, 0),
        ws::param("fm_amount", "FM amount", -1, 1, 0, "", flags, WS2_LINEAR, 5, 4, 0),
        ws::param("timbre_amount", "Timbre amount", -1, 1, 0, "", flags, WS2_LINEAR, 6, 4, 0),
        ws::param("morph_amount", "Morph amount", -1, 1, 0, "", flags, WS2_LINEAR, 7, 4, 0),
        ws::param("decay", "Decay", 0, 1, .5f, "", flags, WS2_LINEAR, 8, 4, 0),
        ws::param("lpg_colour", "LPG colour", 0, 1, .5f, "", flags, WS2_LINEAR, 9, 4, 0)};
    struct Channel {
        plaits::Voice* voice = nullptr;
        uint8_t* memory = nullptr;
        uint32_t rng = 1;
        plaits::Voice::Frame output[12]{};
        uint32_t fill = 0, read = 0;
        plaits::Patch patch{};
        plaits::Modulations modulation{};
    };
    std::array<Channel, 8> channels{};
    uint32_t capacity = 0;
    uint64_t seed = 1;
    static ws2_memory memory_requirements(const ws2_config& c) {
        ws2_memory m{}; m.struct_size = sizeof(m); m.alignment = 32;
        m.fast_bytes = std::min(c.max_channels, 8u) * (sizeof(plaits::Voice) + 32);
        m.large_bytes = std::min(c.max_channels, 8u) * 16384;
        return m;
    }
    bool prepare(const ws::Setup& setup) {
        if (setup.sample_rate() != 48000) return false;
        capacity = std::min(setup.max_channels(), 8u); seed = setup.config.seed;
        for (uint32_t c = 0; c < capacity; ++c) {
            void* object = setup.alloc(sizeof(plaits::Voice), 32, WS2_FAST);
            auto* workspace = static_cast<uint8_t*>(setup.alloc(16384, 32, WS2_LARGE));
            if (!object || !workspace) return false;
            channels[c].voice = new (object) plaits::Voice(); channels[c].memory = workspace;
        }
        return true;
    }
    ~MacroOscillator() { for (auto& ch : channels) if (ch.voice) ch.voice->~Voice(); }
    void reset() {
        for (uint32_t c = 0; c < capacity; ++c) {
            auto& ch = channels[c];
            ch.rng = static_cast<uint32_t>(seed ^ (seed >> 32) ^ (uint64_t(c + 1) * 0x9e3779b9u));
            RngScope scope(ch.rng);
            ch.voice->~Voice();
            std::memset(ch.voice, 0, sizeof(plaits::Voice));
            new (ch.voice) plaits::Voice();
            std::memset(ch.memory, 0, 16384);
            stmlib::BufferAllocator allocator(ch.memory, 16384);
            ch.voice->Init(&allocator);
            ch.fill = ch.read = 0; std::fill_n(ch.output, 12, plaits::Voice::Frame{});
            ch.patch = {}; ch.modulation = {};
        }
    }
    uint32_t latency_frames() const { return 12; }
    void process(ws::Block& b) {
        const uint32_t n = b.channels();
        for (uint32_t out = 0; out < 2; ++out) { b.out(out).set_channels(n); b.out(out).mark_constant(false); }
        for (uint32_t c = 0; c < n; ++c) {
            if (c >= capacity) { for (uint32_t out = 0; out < 2; ++out) std::fill_n(b.out(out).ch(c), b.frames(), 0.f); continue; }
            auto& ch = channels[c];
            for (uint32_t i = 0; i < b.frames(); ++i) {
                b.out(0).ch(c)[i] = ch.output[ch.read].out / 32768.f;
                b.out(1).ch(c)[i] = ch.output[ch.read].aux / 32768.f;
                ch.read = (ch.read + 1) % 12;
                if (ch.fill == 0) {
                    auto p = [&](uint32_t slot) { return b.param(slot).value(c, i); };
                    ch.patch.note = 60.f + p(1); ch.patch.engine = std::clamp(int(std::lround(p(0))), 0, 23);
                    ch.patch.harmonics = p(2); ch.patch.timbre = p(3); ch.patch.morph = p(4);
                    ch.patch.frequency_modulation_amount = p(5); ch.patch.timbre_modulation_amount = p(6); ch.patch.morph_modulation_amount = p(7);
                    ch.patch.decay = p(8); ch.patch.lpg_colour = p(9);
                    ch.modulation.note = std::clamp(12.f * b.in(0).value(c, i), -96.f, 96.f);
                    ch.modulation.engine = std::clamp(b.in(1).value(c, i), -1.f, 1.f);
                    ch.modulation.frequency = std::clamp(b.in(2).value(c, i) * 6.f, -48.f, 48.f);
                    ch.modulation.harmonics = std::clamp(b.in(3).value(c, i), -1.f, 1.f);
                    ch.modulation.timbre = std::clamp(b.in(4).value(c, i), -1.f, 1.f);
                    ch.modulation.morph = std::clamp(b.in(5).value(c, i), -1.f, 1.f);
                    ch.modulation.trigger = std::clamp(b.in(6).value(c, i), 0.f, 1.f);
                    ch.modulation.level = std::clamp(b.in(7).value(c, i), 0.f, 1.f);
                    ch.modulation.frequency_patched = b.in(2).connected(); ch.modulation.timbre_patched = b.in(4).connected();
                    ch.modulation.morph_patched = b.in(5).connected(); ch.modulation.trigger_patched = b.in(6).connected(); ch.modulation.level_patched = b.in(7).connected();
                }
                if (++ch.fill == 12) {
                    RngScope scope(ch.rng); ch.voice->Render(ch.patch, ch.modulation, ch.output, 12); ch.fill = 0;
                }
            }
        }
    }
    int save_state(ws::Writer writer) { return writer.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader reader) { uint32_t version; if (!reader.u32(version)) return WS2_INVALID; if (version != 1) return WS2_UNSUPPORTED; reset(); return WS2_OK; }
};
}
