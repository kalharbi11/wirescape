// SPDX-License-Identifier: GPL-3.0-only
// Original real FFT for sound.spectral: an n/2-point complex radix-2 FFT plus the
// real-split pass. One twiddle table for the largest size serves every smaller size
// by striding. No allocation: the caller hands in storage sized by floats().
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spectral {

// One spectrum: n/2 + 1 bins, real and imaginary rows.
struct Bins {
    float *re, *im;
};
struct ConstBins {
    const float *re, *im;
};

class RealFft {
  public:
    // Floats needed for max_n (power of two): twiddles (max_n/2 cos + sin) and
    // complex work rows (max_n/2 re + im).
    static constexpr size_t floats(uint32_t max_n) { return 2u * size_t(max_n); }

    // max_n: largest transform size. storage: floats(max_n) floats.
    void init(uint32_t max_n, float *storage) {
        max_n_ = max_n;
        const size_t half = max_n / 2;
        cos_ = storage;
        sin_ = storage + half;
        wr_ = storage + 2 * half;
        wi_ = storage + 3 * half;
        const double tau = 6.283185307179586476925286766559;
        for (size_t k = 0; k < half; ++k) { // e^{-2 pi i k / max_n}
            cos_[k] = float(std::cos(tau * double(k) / max_n));
            sin_[k] = float(-std::sin(tau * double(k) / max_n));
        }
    }

    // x: n real samples. out: n/2 + 1 bins (DC .. Nyquist), unnormalised.
    void forward(uint32_t n, const float *x, Bins out) {
        float *re = out.re, *im = out.im;
        const size_t h = n / 2, stride = max_n_ / n;
        for (size_t m = 0; m < h; ++m) {
            wr_[m] = x[2 * m];
            wi_[m] = x[2 * m + 1];
        }
        complex(uint32_t(h), false);
        re[0] = wr_[0] + wi_[0];
        im[0] = 0;
        re[h] = wr_[0] - wi_[0];
        im[h] = 0;
        for (size_t k = 1; k < h; ++k) {
            const float zr = wr_[k], zi = wi_[k], cr = wr_[h - k], ci = -wi_[h - k]; // Z[k], conj Z[h-k]
            const float er = .5f * (zr + cr), ei = .5f * (zi + ci);                  // even part
            const float orr = .5f * (zi - ci), oi = -.5f * (zr - cr);                // odd part: (Z - Zc) / 2i
            const float c = cos_[k * stride], s = sin_[k * stride];
            re[k] = er + c * orr - s * oi;
            im[k] = ei + c * oi + s * orr;
        }
    }

    // in: n/2 + 1 bins. x: n real samples, scaled so inverse(forward(x)) == x.
    void inverse(uint32_t n, ConstBins in, float *x) {
        const float *re = in.re, *im = in.im;
        const size_t h = n / 2, stride = max_n_ / n;
        for (size_t k = 0; k < h; ++k) {
            const float xr = re[k], xi = im[k], cr = re[h - k], ci = -im[h - k]; // X[k], conj X[h-k]
            const float er = .5f * (xr + cr), ei = .5f * (xi + ci);
            const float dr = .5f * (xr - cr), di = .5f * (xi - ci);
            const float c = cos_[k * stride], s = -sin_[k * stride]; // conj twiddle
            const float orr = dr * c - di * s, oi = dr * s + di * c;
            wr_[k] = er - oi; // E + i O
            wi_[k] = ei + orr;
        }
        complex(uint32_t(h), true);
        const float scale = 1.f / float(h);
        for (size_t m = 0; m < h; ++m) {
            x[2 * m] = wr_[m] * scale;
            x[2 * m + 1] = wi_[m] * scale;
        }
    }

  private:
    // In-place iterative radix-2 on wr_/wi_, size h. inverse: conjugate twiddles, no scale.
    void complex(uint32_t h, bool inverse) {
        for (uint32_t i = 1, j = 0; i < h; ++i) {
            uint32_t bit = h >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) {
                float t = wr_[i];
                wr_[i] = wr_[j];
                wr_[j] = t;
                t = wi_[i];
                wi_[i] = wi_[j];
                wi_[j] = t;
            }
        }
        const float sign = inverse ? -1.f : 1.f;
        for (uint32_t len = 2; len <= h; len <<= 1) {
            const size_t half = len >> 1, step = max_n_ / len; // e^{-2 pi i k / len} = table[k * max_n / len]
            for (uint32_t i = 0; i < h; i += len)
                for (size_t k = 0; k < half; ++k) {
                    const float c = cos_[k * step], s = sign * sin_[k * step];
                    const size_t a = i + k, b = a + half;
                    const float tr = wr_[b] * c - wi_[b] * s, ti = wr_[b] * s + wi_[b] * c;
                    wr_[b] = wr_[a] - tr;
                    wi_[b] = wi_[a] - ti;
                    wr_[a] += tr;
                    wi_[a] += ti;
                }
        }
    }
    uint32_t max_n_ = 0;
    float *cos_ = nullptr, *sin_ = nullptr, *wr_ = nullptr, *wi_ = nullptr;
};

} // namespace spectral
