<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Noise design
Source: `native/modules/noise/module.cpp` at main `f23ea96`. Xorshift32 advances even while muted. Channel 0 uses 0x12345678 exactly; channel c uses 0x12345678 XOR (c * 0x9e3779b9), unsigned wrap, replacing zero with one. No engine seed is read. Upper 24 bits and double 0.2 default/ramp math match ABI1. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Provenance: the existing ABI1 DESIGN.md records original DSP, with no external third-party implementation or pinned external revision. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
