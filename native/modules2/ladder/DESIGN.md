<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Ladder Filter design
Source: `native/modules/ladder/module.cpp` at main `f23ea96`. The four private double poles, previous-stage feedback, exact tan/tanh calls, double parameter ramps, linear drive=0 branch, finite-input handling and explicit 1e-20 tail cut are copied in the original operation order. No voice SIMD or coefficient approximation is introduced. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Provenance: the existing ABI1 DESIGN.md records original DSP with no external third-party implementation or external pinned revision. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
