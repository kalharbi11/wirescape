<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Slew design
Source: `native/modules/slew/module.cpp` at main `f23ea96`. The held level and default 0.1-second rates remain double. Rise/fall changes alter the step limit immediately, with no extra amplitude ramp. Missing input slews toward zero; nonfinite targets hold state. Zero time jumps directly to the target. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Provenance: the existing ABI1 DESIGN.md records original DSP and no external third-party source or external pinned revision. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
