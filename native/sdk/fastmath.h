// SPDX-License-Identifier: MIT
#pragma once
namespace ws::fastmath {
// Opt-in only. Range |x|<=0.25 radians, tested absolute error <6e-8.
// Deliberately not used by the VCF port: its range reaches .45*pi.
inline float tan_small(float x) noexcept {
    const double y=x,z=y*y;
    return static_cast<float>(y*(1+z*(1.0/3+z*(2.0/15+z*(17.0/315+z*(62.0/2835))))));
}
}
