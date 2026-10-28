<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Clock design
Source: `native/modules/clock/module.cpp` at main `f23ea96`. Free-running double phase with the ABI1 epsilon at duty boundaries. BPM retains the 5 ms ramp; tick lengths round with floor(value + 0.5), run thresholds at 0.5, and reset changes phase on a rising >=0.5 edge only. No transport sync is added. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Provenance: the existing ABI1 DESIGN.md records original DSP, no external third-party implementation and no external pinned revision. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
