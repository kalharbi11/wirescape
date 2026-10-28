<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Output design
Source: `native/modules/output/module.cpp` at main `f23ea96`. Transparent per-voice audio output; a missing input is zero. It opens no device. Engine2 output(node, port, bus_channel) mixes the voice channels into the chosen bus once; this module does not pre-sum voices or introduce a second bus reduction. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Original Wirescape code, GPL-3.0-only, no upstream source. No ABI1 DESIGN.md exists; only the source implementation is established. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
