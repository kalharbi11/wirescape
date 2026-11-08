// SPDX-License-Identifier: GPL-3.0-only
// essentials.quantizer: polyphonic pitch quantizer with hysteresis, scale shift and a change trigger.
#pragma once
#include "wirescape_sdk.h"

namespace quantizer {

// Pitch classes (0..11) of a scale as a bit mask, built from semitone intervals above the root.
template <int... Semitones> constexpr uint16_t scale_bits() { return static_cast<uint16_t>(((1u << Semitones) | ...)); }

// A stepped switch: no host smoothing, integer values, no cable modulation.
constexpr ws2_param stepped(const char *id, const char *name, float lo, float hi, float def, uint32_t slot) {
    return ws::param(id, name, lo, hi, def, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, slot, 4, 1);
}

struct Quantizer {
    static constexpr ws::Info info{"essentials.quantizer", "Quantizer", 1};
    static constexpr std::array inputs{ws::port("pitch", "Pitch", WS2_PITCH, WS2_POLY),
                                       ws::port("trigger", "Trigger", WS2_TRIGGER, WS2_POLY)};
    static constexpr std::array outputs{ws::port("pitch", "Pitch", WS2_PITCH),
                                        ws::port("changed", "Changed", WS2_TRIGGER)};
    // Slots: 0 scale, 1 root, 2 shift, 3 octave, 4..15 custom notes C..B.
    static constexpr std::array params{stepped("scale", "Scale", 0, 13, 1, 0), stepped("root", "Root", 0, 11, 0, 1),
                                       stepped("shift", "Shift", -7, 7, 0, 2), stepped("octave", "Octave", -3, 3, 0, 3),
                                       stepped("n0", "C", 0, 1, 1, 4),         stepped("n1", "C#", 0, 1, 0, 5),
                                       stepped("n2", "D", 0, 1, 1, 6),         stepped("n3", "D#", 0, 1, 0, 7),
                                       stepped("n4", "E", 0, 1, 1, 8),         stepped("n5", "F", 0, 1, 1, 9),
                                       stepped("n6", "F#", 0, 1, 0, 10),       stepped("n7", "G", 0, 1, 1, 11),
                                       stepped("n8", "G#", 0, 1, 0, 12),       stepped("n9", "A", 0, 1, 1, 13),
                                       stepped("n10", "A#", 0, 1, 0, 14),      stepped("n11", "B", 0, 1, 1, 15)};

    static constexpr uint32_t kMaxVoices = 16;
    static constexpr int kCustom = 13;
    static constexpr double kHysteresis = 0.1; // semitones past the midpoint to the next allowed note
    static constexpr double kLimit = 1200;     // input clamp in semitones; keeps all integer math small
    // Interval masks above the root, in the order of the Scale parameter (Custom has no table).
    static constexpr std::array<uint16_t, 13> tables{scale_bits<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11>(),
                                                     scale_bits<0, 2, 4, 5, 7, 9, 11>(),
                                                     scale_bits<0, 2, 3, 5, 7, 8, 10>(),
                                                     scale_bits<0, 2, 3, 5, 7, 8, 11>(),
                                                     scale_bits<0, 2, 3, 5, 7, 9, 10>(),
                                                     scale_bits<0, 1, 3, 5, 7, 8, 10>(),
                                                     scale_bits<0, 2, 4, 6, 7, 9, 11>(),
                                                     scale_bits<0, 2, 4, 5, 7, 9, 10>(),
                                                     scale_bits<0, 1, 3, 5, 6, 8, 10>(),
                                                     scale_bits<0, 2, 4, 7, 9>(),
                                                     scale_bits<0, 3, 5, 7, 10>(),
                                                     scale_bits<0, 3, 5, 6, 7, 10>(),
                                                     scale_bits<0, 2, 4, 6, 8, 10>()};

    // Per block settings, read once at sample 0 (all are stepped switches).
    struct Config {
        uint16_t mask = 0xFFF;
        int shift = 0;
        int octave = 0;
    };

    struct Voice {
        ws::Schmitt schmitt;
        ws::Trigger changed;
        bool high = false;      // Schmitt output last sample (Schmitt has no getter)
        bool has = false;       // a note is held
        bool active = false;    // the last `changed` sample was high, so a pulse may still be running
        uint16_t mask = 0;      // mask the hold region below was computed for
        int note = 0;           // held quantized note, semitones from C4, before shift/octave
        int out_note = 0;       // held output note, semitones from C4
        double low = 0, up = 0; // hold region of `note`, in semitones
        float pitch = 0;        // held output in 1.0/oct
    };

    std::array<Voice, kMaxVoices> voices{};
    double rate = 48000;

    void prepare(const ws::Setup &s) {
        rate = s.sample_rate();
        for (Voice &v : voices) v.changed.prepare(rate);
    }

    // Held notes are not state: reset forgets them.
    void reset() {
        for (Voice &v : voices) {
            v = Voice{};
            v.changed.prepare(rate);
        }
    }

    static int floor_div(int a, int b) noexcept { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }
    static bool allowed(uint16_t mask, int note) noexcept { return (mask >> (note - 12 * floor_div(note, 12))) & 1u; }
    static int count(uint16_t mask) noexcept {
        int n = 0;
        for (int p = 0; p < 12; ++p) n += (mask >> p) & 1;
        return n;
    }

    // Nearest allowed note; ties go down. The mask is never empty, so both searches end within 12 steps.
    static int nearest(double x, const Config &cfg) noexcept {
        const int f = static_cast<int>(std::floor(x));
        int lo = f, hi = f + 1;
        for (int k = 0; k < 12; ++k)
            if (allowed(cfg.mask, f - k)) {
                lo = f - k;
                break;
            }
        for (int k = 1; k <= 12; ++k)
            if (allowed(cfg.mask, f + k)) {
                hi = f + k;
                break;
            }
        return (hi - x) < (x - lo) ? hi : lo;
    }

    // Move `shift` allowed notes up (or down), wrapping into the next octave.
    static int shifted(int note, const Config &cfg) noexcept {
        if (cfg.shift == 0) return note;
        const int octave = floor_div(note, 12), pc = note - 12 * octave, k = count(cfg.mask);
        const int index = count(static_cast<uint16_t>(cfg.mask & ((1u << pc) - 1))) + cfg.shift;
        const int wraps = floor_div(index, k), degree = index - wraps * k;
        int seen = 0;
        for (int p = 0; p < 12; ++p)
            if ((cfg.mask >> p) & 1u) {
                if (seen++ == degree) return 12 * (octave + wraps) + p;
            }
        return note;
    }

    // Hold `note` until the input passes the midpoint to a neighbour by kHysteresis.
    static void set_hold(Voice &v, int note, uint16_t mask) noexcept {
        int below = note - 12, above = note + 12;
        for (int k = 1; k <= 12; ++k)
            if (allowed(mask, note - k)) {
                below = note - k;
                break;
            }
        for (int k = 1; k <= 12; ++k)
            if (allowed(mask, note + k)) {
                above = note + k;
                break;
            }
        v.note = note;
        v.mask = mask;
        v.low = (below + note) * 0.5 - kHysteresis;
        v.up = (note + above) * 0.5 + kHysteresis;
    }

    // Quantize one input value (semitones from C4). Returns true when an already held output note changed.
    static bool update(Voice &v, double x, const Config &cfg) noexcept {
        if (!(x == x)) x = 0;
        x = std::clamp(x, -kLimit, kLimit);
        if (!(v.has && v.mask == cfg.mask && x >= v.low && x <= v.up)) set_hold(v, nearest(x, cfg), cfg.mask);
        const int out = shifted(v.note, cfg) + 12 * cfg.octave;
        const bool changed = v.has && out != v.out_note;
        v.has = true;
        v.out_note = out;
        v.pitch = static_cast<float>(out / 12.0); // exact for every allowed note
        return changed;
    }

    static Config read_config(ws::Block &b) noexcept {
        auto param = [&](uint32_t i, long lo, long hi) {
            return static_cast<int>(std::clamp<long>(std::lround(b.param(i).value(0, 0)), lo, hi));
        };
        Config cfg;
        const int scale = param(0, 0, kCustom), root = param(1, 0, 11);
        cfg.shift = param(2, -7, 7);
        cfg.octave = param(3, -3, 3);
        uint16_t mask = 0;
        if (scale == kCustom) {
            for (int k = 0; k < 12; ++k)
                if (param(4 + k, 0, 1)) mask |= static_cast<uint16_t>(1u << k);
        } else {
            for (int p = 0; p < 12; ++p)
                if ((tables[scale] >> ((p - root + 12) % 12)) & 1u) mask |= static_cast<uint16_t>(1u << p);
        }
        cfg.mask = mask ? mask : 0xFFF; // an empty Custom scale passes Chromatic
        return cfg;
    }

    void process(ws::Block &b) {
        const uint32_t n = std::min(b.channels(), kMaxVoices), frames = b.frames();
        b.out(0).set_channels(n);
        b.out(1).set_channels(n);
        const Config cfg = read_config(b);
        const bool gated = b.in(1).connected();
        const bool constant = b.in(0).constant() && (!gated || b.in(1).constant());
        if (constant) {
            // Constant inputs can only act at sample 0; the held note then lasts the whole block.
            bool quiet = true;
            bool fired[kMaxVoices]{};
            for (uint32_t c = 0; c < n; ++c) {
                Voice &v = voices[c];
                bool sample = true;
                if (gated) {
                    const bool now = v.schmitt.process(b.in(1).ch(c)[0]);
                    sample = !v.has || (now && !v.high);
                    v.high = now;
                }
                fired[c] = sample && update(v, b.in(0).ch(c)[0] * 12.0, cfg);
                b.out(0).ch(c)[0] = v.pitch;
                quiet = quiet && !fired[c] && !v.active;
            }
            b.out(0).mark_constant(true);
            b.out(1).mark_constant(quiet);
            for (uint32_t c = 0; c < n; ++c) {
                Voice &v = voices[c];
                float *out = b.out(1).ch(c);
                if (quiet) {
                    out[0] = 0;
                    continue;
                }
                if (fired[c]) v.changed.fire();
                for (uint32_t i = 0; i < frames; ++i) out[i] = v.changed.next();
                v.active = out[frames - 1] != 0;
            }
            return;
        }
        b.out(0).mark_constant(false);
        b.out(1).mark_constant(false);
        for (uint32_t c = 0; c < n; ++c) {
            Voice &v = voices[c];
            const auto pitch = b.in(0).ch(c), trigger = b.in(1).ch(c);
            float *out = b.out(0).ch(c);
            float *changed = b.out(1).ch(c);
            for (uint32_t i = 0; i < frames; ++i) {
                bool sample = true;
                if (gated) {
                    const bool now = v.schmitt.process(trigger[i]);
                    sample = !v.has || (now && !v.high);
                    v.high = now;
                }
                if (sample && update(v, pitch[i] * 12.0, cfg)) v.changed.fire();
                out[i] = v.pitch;
                changed[i] = v.changed.next();
            }
            v.active = changed[frames - 1] != 0;
        }
    }
};

} // namespace quantizer
