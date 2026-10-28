<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Scale/Offset design
Source: `native/modules/scale_offset/module.cpp` at main `f23ea96`. Output is input times the double scale ramp plus the double offset ramp, preserving operation order. Missing input still outputs the smoothed offset; reset snaps both ramps to their user targets. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Provenance: to be confirmed by the user. No ABI1 DESIGN.md exists; only the source implementation is established. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
