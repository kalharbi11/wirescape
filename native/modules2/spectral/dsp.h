// SPDX-License-Identifier: GPL-3.0-only
// sound.spectral: one streaming STFT engine shared by several two-source spectral processes.
// Concept from the owner's Conjunctions prototype; original code. See DESIGN.md.
#pragma once
#include "fft.h"
#include "wirescape_sdk.h"
#include <atomic>

namespace spectral {

enum Mode : uint8_t { Morph, Cross, Max, Mean, Min, Diff, kModes };
enum Param : uint8_t { P_MODE, P_SIZE, P_MORPH, P_CURVE, P_PITCH, P_AMOUNT, P_SMOOTH, P_BLEVEL, P_LEVEL, kParams };
constexpr uint32_t kBaseSize[4] = {512, 1024, 2048, 4096}; // at 44.1/48 kHz
constexpr uint32_t kDefaultSize = 2, kFadeFrames = 4, kStateVersion = 1;
constexpr uint32_t kStepMod = WS2_STEPPED | WS2_MODULATABLE | WS2_AUTOMATABLE, kStep = WS2_STEPPED | WS2_AUTOMATABLE;
constexpr float kTau = 6.28318530717958647692f;

// FFT sizes double at 88.2/96 kHz (and again above 132 kHz), so resolution in Hz and time smear match.
inline uint32_t rate_factor(double rate) { return rate > 132000 ? 4u : rate > 66000 ? 2u : 1u; }
inline uint32_t max_size(double rate) { return kBaseSize[3] * rate_factor(rate); }
// Floats: FFT tables + work, window, rings A/B, OLA, frame, then 17 bin rows (+1 for the prefix sum).
inline size_t storage_floats(double rate) {
    const size_t n = max_size(rate), k = n / 2 + 1;
    return RealFft::floats(uint32_t(n)) + 5 * n + 17 * k + 1;
}
inline float principal(float x) { return x - kTau * std::floor(x / kTau + .5f); }
inline float magnitude(float re, float im) { return std::sqrt(re * re + im * im); }

struct Spectral {
    static constexpr ws::Info info{"sound.spectral", "Spectral", 1};
    static constexpr std::array inputs{ws::port("a", "A", WS2_AUDIO, WS2_SUM), ws::port("b", "B", WS2_AUDIO, WS2_SUM)};
    static constexpr std::array outputs{ws::port("audio", "Audio", WS2_AUDIO)};
    static constexpr std::array params{
        ws::param("mode", "Mode", 0, kModes - 1, Morph, "", kStepMod, WS2_LINEAR, P_MODE, 4, 1),
        ws::param("size", "Resolution", 0, 3, kDefaultSize, "", kStep, WS2_LINEAR, P_SIZE, 4, 1),
        ws::param("morph", "Morph", 0, 1, .5f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, P_MORPH),
        ws::param("curve", "Morph curve", 0, 1, 0, "", kStep, WS2_LINEAR, P_CURVE, 4, 1),
        ws::param("pitch", "Morph pitch", 0, 1, 0, "", kStep, WS2_LINEAR, P_PITCH, 4, 1),
        ws::param("amount", "Cross amount", 0, 1, 1, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, P_AMOUNT),
        ws::param("smooth", "Cross smoothing", 0, 1, .15f, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, P_SMOOTH),
        ws::param("b_level", "B level", 0, 2, 1, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, P_BLEVEL),
        ws::param("level", "Level", 0, 2, 1, "", WS2_MODULATABLE | WS2_AUTOMATABLE, WS2_LINEAR, P_LEVEL)};

    static ws2_memory memory_requirements(const ws2_config &c) {
        return {sizeof(ws2_memory), 0, storage_floats(c.sample_rate) * sizeof(float), 64};
    }
    bool prepare(const ws::Setup &s) {
        factor_ = rate_factor(s.sample_rate());
        nmax_ = max_size(s.sample_rate());
        mask_ = nmax_ - 1;
        float *p = static_cast<float *>(s.alloc(storage_floats(s.sample_rate()) * sizeof(float), 64, WS2_LARGE));
        if (!p) return false;
        const uint32_t k = nmax_ / 2 + 1;
        fft_.init(nmax_, p);
        p += RealFft::floats(nmax_);
        hann_ = p;
        p += nmax_;
        for (uint32_t j = 0; j < nmax_; ++j) hann_[j] = float(.5 - .5 * std::cos(6.283185307179586 * j / nmax_));
        ring_a_ = p;
        p += nmax_;
        ring_b_ = p;
        p += nmax_;
        ola_ = p;
        p += nmax_;
        frame_ = p;
        p += nmax_;
        float **rows[] = {&ar_,   &ai_,   &br_,   &bi_,  &yr_,  &yi_, &zr_, &zi_,    &ua_r_,
                          &ua_i_, &ub_r_, &ub_i_, &p_r_, &p_i_, &ea_, &eb_, &prefix_};
        for (float **r : rows) {
            *r = p;
            p += k;
        }
        seed_ = s.config.seed;
        reset();
        return true;
    }
    void reset() {
        std::fill_n(ring_a_, 3 * nmax_, 0.f); // rings A, B and the OLA are contiguous
        clear_bins();
        write_ = 0;
        silent_a_ = silent_b_ = nmax_;
        live_ = 0;
        mode_ = prev_mode_ = Morph;
        fade_ = 0;
        drain_ = 0;
        set_size(kDefaultSize);
        pending_ = size_idx_;
        rng_.seed(seed_);
    }
    uint32_t latency_frames() const { return latency_.load(std::memory_order_relaxed); }

    void process(ws::Block &b) {
        auto out = b.out(0);
        out.set_channels(1);
        float *y = out.ch(0);
        const uint32_t frames = b.frames();
        const ws::Channel a = b.in(0).ch(0), bb = b.in(1).ch(0), level = b.param(P_LEVEL).ch(0);
        bool zero = true;
        for (uint32_t i = 0; i < frames; ++i) {
            if (drain_ == 0 && hop_pos_ == 0) tick(b, i);
            const float v = ola_[write_] * level[i];
            ola_[write_] = 0;
            y[i] = v;
            zero &= v == 0;
            const float xa = a[i], xb = bb[i];
            ring_a_[write_] = xa;
            ring_b_[write_] = xb;
            silent_a_ = xa == 0 ? std::min(silent_a_ + 1, nmax_) : 0;
            silent_b_ = xb == 0 ? std::min(silent_b_ + 1, nmax_) : 0;
            write_ = (write_ + 1) & mask_;
            if (live_) --live_;
            if (drain_) {
                if (--drain_ == 0) {
                    set_size(pending_);
                    hop_pos_ = 0;
                }
            } else if (++hop_pos_ == hop_) hop_pos_ = 0;
        }
        // Constant only when nothing moves: both inputs constant and the whole block came out silent.
        out.mark_constant(zero && b.in(0).constant() && b.in(1).constant());
    }

    int save_state(ws::Writer w) {
        const ws::Random::State r = rng_.save();
        const uint32_t words[] = {kStateVersion,
                                  hop_pos_,
                                  mode_,
                                  prev_mode_,
                                  fade_,
                                  size_idx_,
                                  drain_,
                                  pending_,
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
        uint32_t v[11];
        for (uint32_t &x : v)
            if (!r.u32(x)) return WS2_INVALID;
        const uint32_t hop_pos = v[0], mode = v[1], prev = v[2], fade = v[3], size = v[4], drain = v[5], pending = v[6];
        if (mode >= kModes || prev >= kModes || fade > kFadeFrames || size > 3 || pending > 3) return WS2_INVALID;
        const uint32_t n = kBaseSize[size] * factor_;
        if (hop_pos >= n / 4 || drain > n - n / 4 || (drain && pending == size)) return WS2_INVALID;
        if (!(v[9] & 1u)) return WS2_INVALID; // PCG streams are odd
        // Audio history is not saved: restart the analysis on the saved schedule.
        reset();
        set_size(size);
        hop_pos_ = hop_pos;
        mode_ = mode;
        prev_mode_ = prev;
        fade_ = fade;
        drain_ = drain;
        pending_ = drain ? pending : size;
        rng_.restore({uint64_t(v[7]) | uint64_t(v[8]) << 32, uint64_t(v[9]) | uint64_t(v[10]) << 32});
        return WS2_OK;
    }

  private:
    // A stepped control at block offset i, rounded and clamped to its declared range.
    static uint32_t stepped(const ws::Block &b, Param p, uint32_t i) {
        const long top = std::lround(params[p].maximum);
        const long v = std::lround(b.param(p).value(0, i));
        return uint32_t(std::clamp<long>(v, 0, long(top)));
    }
    void set_size(uint32_t idx) {
        size_idx_ = idx;
        n_ = kBaseSize[idx] * factor_;
        hop_ = n_ / 4;
        bins_ = n_ / 2 + 1;
        hop_pos_ = 0;
        clear_bins();
        latency_.store(n_, std::memory_order_relaxed);
    }
    // Phase-vocoder state as unit phasors: previous A, previous B and the output phase, all at angle 0.
    void clear_bins() {
        const uint32_t k = nmax_ / 2 + 1;
        for (float *re : {ua_r_, ub_r_, p_r_}) std::fill_n(re, k, 1.f);
        for (float *im : {ua_i_, ub_i_, p_i_}) std::fill_n(im, k, 0.f);
    }
    void enter(uint32_t mode) {
        if (mode == Morph) clear_bins();
    }

    // One hop boundary at block offset i: read controls, analyse, render, overlap-add.
    void tick(const ws::Block &b, uint32_t i) {
        const uint32_t size = stepped(b, P_SIZE, i);
        if (size != size_idx_) {
            if (live_ == 0) set_size(size); // nothing sounding: switch at once
            else {
                pending_ = size;
                drain_ = n_ - hop_;
                return;
            } // let the old frames fade out first
        }
        const uint32_t target = stepped(b, P_MODE, i);
        if (fade_ == 0 && target != mode_) {
            prev_mode_ = mode_;
            mode_ = target;
            fade_ = kFadeFrames;
            enter(mode_);
        }
        if (silent_a_ >= n_ && silent_b_ >= n_) { // silent-frame rule: same in every code path
            clear_bins();
            if (fade_) --fade_;
            return;
        }
        analyse(ring_a_, ar_, ai_);
        analyse(ring_b_, br_, bi_);
        const Controls c{b.param(P_MORPH).value(0, i),  stepped(b, P_CURVE, i),        stepped(b, P_PITCH, i),
                         b.param(P_AMOUNT).value(0, i), b.param(P_SMOOTH).value(0, i), b.param(P_BLEVEL).value(0, i)};
        render(mode_, c, yr_, yi_);
        if (fade_) {
            render(prev_mode_, c, zr_, zi_);
            const float g = float(kFadeFrames + 1 - fade_) / float(kFadeFrames + 1);
            for (uint32_t k = 0; k < bins_; ++k) {
                yr_[k] = g * yr_[k] + (1 - g) * zr_[k];
                yi_[k] = g * yi_[k] + (1 - g) * zi_[k];
            }
            --fade_;
        }
        yi_[0] = 0;
        yi_[bins_ - 1] = 0;
        fft_.inverse(n_, {yr_, yi_}, frame_);
        // Hann analysis (scaled 4/N) x Hann synthesis (scaled N/4 / 1.5) sums to 1 at hop N/4.
        const size_t stride = nmax_ / n_;
        for (uint32_t j = 0; j < n_; ++j) ola_[(write_ + j) & mask_] += frame_[j] * hann_[j * stride] * (1.f / 1.5f);
        live_ = n_;
    }
    // Last n_ input samples, windowed and scaled so a full-scale sine peaks near 1 in its bin.
    void analyse(const float *ring, float *re, float *im) {
        const size_t stride = nmax_ / n_;
        const uint32_t start = (write_ - n_) & mask_;
        const float scale = 4.f / float(n_);
        for (uint32_t j = 0; j < n_; ++j) frame_[j] = ring[(start + j) & mask_] * hann_[j * stride] * scale;
        fft_.forward(n_, frame_, {re, im});
    }

    struct Controls {
        float morph;
        uint32_t curve, pitch;
        float amount, smooth, b_level;
    };
    void render(uint32_t mode, const Controls &c, float *yr, float *yi) {
        // Synthesis gain N/4 restores the analysis scale; folded into every write below.
        const float s = float(n_) / 4.f;
        switch (mode) {
        case Morph:
            morph(c, s, yr, yi);
            break;
        case Cross:
            cross(c, s, yr, yi);
            break;
        default:
            combine(Mode(mode), c, s, yr, yi);
            break;
        }
    }
    // Phase vocoder. Each bin's frequency is its phase advance per hop; the output phase accumulates
    // the advance of the louder source (exact as a phasor product) or a blend of both (needs unwrapping).
    void morph(const Controls &c, float s, float *yr, float *yi) {
        const float m = std::clamp(c.morph, 0.f, 1.f);
        for (uint32_t k = 0; k < bins_; ++k) {
            const float x = magnitude(ar_[k], ai_[k]), y = magnitude(br_[k], bi_[k]);
            const float ur = x > 0 ? ar_[k] / x : 1.f, ui = x > 0 ? ai_[k] / x : 0.f;
            const float vr = y > 0 ? br_[k] / y : 1.f, vi = y > 0 ? bi_[k] / y : 0.f;
            const float ra = ur * ua_r_[k] + ui * ua_i_[k], ia = ui * ua_r_[k] - ur * ua_i_[k]; // u * conj(previous)
            const float rb = vr * ub_r_[k] + vi * ub_i_[k], ib = vi * ub_r_[k] - vr * ub_i_[k];
            ua_r_[k] = ur;
            ua_i_[k] = ui;
            ub_r_[k] = vr;
            ub_i_[k] = vi;
            float rr, ri;
            if (c.pitch == 1) {
                const float expected = kTau * .25f * float(k); // advance per hop at overlap 4
                const float adv_a = expected + principal(std::atan2(ia, ra) - expected);
                const float adv_b = expected + principal(std::atan2(ib, rb) - expected);
                const float adv = adv_a * (1 - m) + adv_b * m;
                rr = std::cos(adv);
                ri = std::sin(adv);
            } else if ((1 - m) * x >= m * y) {
                rr = ra;
                ri = ia;
            } else {
                rr = rb;
                ri = ib;
            }
            const float pr = p_r_[k] * rr - p_i_[k] * ri, pi = p_r_[k] * ri + p_i_[k] * rr;
            const float norm = 1.f / std::max(magnitude(pr, pi), 1e-20f);
            p_r_[k] = pr * norm;
            p_i_[k] = pi * norm;
            const float mag =
                c.curve == 0 ? std::exp((1 - m) * std::log(x + 1e-7f) + m * std::log(y + 1e-7f)) : x * (1 - m) + y * m;
            yr[k] = s * mag * p_r_[k];
            yi[k] = s * mag * p_i_[k];
        }
    }
    // Constant-Q-ish moving average of a magnitude row (in place), via a prefix sum.
    void smooth_row(float *row, float amount) {
        if (amount <= 0) return;
        prefix_[0] = 0;
        for (uint32_t k = 0; k < bins_; ++k) prefix_[k + 1] = prefix_[k] + row[k];
        for (uint32_t k = 0; k < bins_; ++k) {
            const uint32_t h = uint32_t(amount * (1 + float(k) * .18f));
            const uint32_t lo = k > h ? k - h : 0, hi = std::min(bins_, k + h + 1);
            row[k] = (prefix_[hi] - prefix_[lo]) / float(hi - lo);
        }
    }
    void cross(const Controls &c, float s, float *yr, float *yi) {
        for (uint32_t k = 0; k < bins_; ++k) {
            ea_[k] = magnitude(ar_[k], ai_[k]);
            eb_[k] = magnitude(br_[k], bi_[k]);
        }
        const float width = std::clamp(c.smooth, 0.f, 1.f) * 24.f, amount = std::clamp(c.amount, 0.f, 1.f);
        smooth_row(ea_, width);
        smooth_row(eb_, width);
        for (uint32_t k = 0; k < bins_; ++k) {
            const float ratio = std::min(std::exp(amount * std::log((ea_[k] + 1e-6f) / (eb_[k] + 1e-6f))), 1e3f);
            yr[k] = s * br_[k] * ratio;
            yi[k] = s * bi_[k] * ratio;
        }
    }
    void combine(Mode mode, const Controls &c, float s, float *yr, float *yi) {
        const float gb = std::clamp(c.b_level, 0.f, 2.f);
        for (uint32_t k = 0; k < bins_; ++k) {
            const float x = magnitude(ar_[k], ai_[k]), y = magnitude(br_[k], bi_[k]) * gb;
            const bool a_louder = x >= y;
            float re = 0, im = 0;
            if (mode == Max) {
                if (a_louder) {
                    re = ar_[k];
                    im = ai_[k];
                } else {
                    re = br_[k] * gb;
                    im = bi_[k] * gb;
                }
            } else if (mode == Mean) { // average magnitude, louder bin's phase
                const float m = .5f * (x + y), loud = a_louder ? x : y;
                const float g = loud > 0 ? m / loud : 0;
                re = (a_louder ? ar_[k] : br_[k]) * g;
                im = (a_louder ? ai_[k] : bi_[k]) * g;
            } else { // Min / A - B keep A's phase
                const float m = mode == Min ? std::min(x, y) : std::max(0.f, x - y);
                const float g = x > 0 ? m / x : 0;
                re = ar_[k] * g;
                im = ai_[k] * g;
            }
            yr[k] = s * re;
            yi[k] = s * im;
        }
    }

    RealFft fft_;
    float *hann_ = nullptr, *ring_a_ = nullptr, *ring_b_ = nullptr, *ola_ = nullptr, *frame_ = nullptr;
    float *ar_ = nullptr, *ai_ = nullptr, *br_ = nullptr, *bi_ = nullptr, *yr_ = nullptr, *yi_ = nullptr;
    float *zr_ = nullptr, *zi_ = nullptr, *ua_r_ = nullptr, *ua_i_ = nullptr, *ub_r_ = nullptr, *ub_i_ = nullptr;
    float *p_r_ = nullptr, *p_i_ = nullptr;
    float *ea_ = nullptr, *eb_ = nullptr, *prefix_ = nullptr;
    uint32_t factor_ = 1, nmax_ = 0, mask_ = 0, n_ = 0, hop_ = 1, bins_ = 0, size_idx_ = kDefaultSize,
             pending_ = kDefaultSize;
    uint32_t write_ = 0, hop_pos_ = 0, silent_a_ = 0, silent_b_ = 0, live_ = 0, drain_ = 0;
    uint32_t mode_ = Morph, prev_mode_ = Morph, fade_ = 0;
    uint64_t seed_ = 0;
    ws::Random rng_;
    std::atomic<uint32_t> latency_{kBaseSize[kDefaultSize]};
};

} // namespace spectral
