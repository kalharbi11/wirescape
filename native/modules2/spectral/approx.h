// SPDX-License-Identifier: GPL-3.0-only
// Small, branch-light float approximations for the per-bin loops of sound.spectral (original code).
// Accuracy (checked in tests.cpp against <cmath>): log2 and exp2 within 2e-5 relative,
// atan2 within 2e-5 rad, sin/cos within 4e-6 on [-pi, pi]. Inputs are finite.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spectral::approx {

constexpr float kPi = 3.14159265358979323846f, kHalfPi = 1.57079632679489661923f;

// log2(x) for x > 0: exponent from the bits, mantissa folded to [sqrt(1/2), sqrt(2)), then the
// atanh series in y = (m - 1) / (m + 1), |y| <= 0.1716.
inline float log2(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof bits);
    int exponent = int((bits >> 23) & 0xffu) - 127;
    bits = (bits & 0x007fffffu) | 0x3f800000u; // mantissa in [1, 2)
    float m;
    std::memcpy(&m, &bits, sizeof m);
    if (m > 1.41421356f) {
        m *= .5f;
        ++exponent;
    }
    const float y = (m - 1.f) / (m + 1.f), y2 = y * y;
    const float series = y * (2.f + y2 * (2.f / 3.f + y2 * (2.f / 5.f + y2 * (2.f / 7.f))));
    return float(exponent) + series * 1.44269504088896340736f;
}

// 2^x, clamped to the normal float range; integer part into the exponent, |fraction| <= 0.5.
inline float exp2(float x) {
    x = x < -126.f ? -126.f : x > 126.f ? 126.f : x;
    const float whole = std::floor(x + .5f), f = (x - whole) * .69314718055994530942f;
    const float p = 1.f + f * (1.f + f * (.5f + f * (1.f / 6.f + f * (1.f / 24.f + f * (1.f / 120.f + f / 720.f)))));
    const uint32_t bits = uint32_t(int(whole) + 127) << 23;
    float scale;
    std::memcpy(&scale, &bits, sizeof scale);
    return p * scale;
}

// atan2(y, x) in [-pi, pi]; atan2(0, 0) = 0.
inline float atan2(float y, float x) {
    const float ax = std::abs(x), ay = std::abs(y), hi = ax > ay ? ax : ay, lo = ax > ay ? ay : ax;
    if (hi == 0) return 0;
    const float a = lo / hi, s = a * a;
    float r = a * (.99997726f +
                   s * (-.33262347f + s * (.19354346f + s * (-.11643287f + s * (.05265332f + s * -.01172120f)))));
    if (ay > ax) r = kHalfPi - r;
    if (x < 0) r = kPi - r;
    return y < 0 ? -r : r;
}

// sin and cos of t in [-pi, pi]: fold to [-pi/2, pi/2], then Taylor to degree 9 / 10.
inline void sincos(float t, float &sine, float &cosine) {
    float sign = 1.f;
    if (t > kHalfPi) {
        t = kPi - t;
        sign = -1.f;
    } else if (t < -kHalfPi) {
        t = -kPi - t;
        sign = -1.f;
    }
    const float t2 = t * t;
    sine = t * (1.f + t2 * (-1.f / 6.f + t2 * (1.f / 120.f + t2 * (-1.f / 5040.f + t2 * (1.f / 362880.f)))));
    cosine =
        sign *
        (1.f + t2 * (-.5f + t2 * (1.f / 24.f + t2 * (-1.f / 720.f + t2 * (1.f / 40320.f + t2 * (-1.f / 3628800.f))))));
}

} // namespace spectral::approx
