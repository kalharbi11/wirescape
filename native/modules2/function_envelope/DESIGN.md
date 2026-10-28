<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Function Envelope design
Source: `native/modules/function_envelope/module.cpp` at main `f23ea96`. Double held/start/end levels, edge-latched curve and exact expm1 shaping are retained. Gate changes start a rounded sample-count segment from the current level, including falling from a held nonzero level. No note-event or transport semantics are added. Per-voice state is fixed; process/reset have no allocation, locks or I/O. Continuous controls are per-voice D7 parameters. Plain-unit ABI1 ramps are retained where present; the host also applies D7 base smoothing. Version-1 state uses explicit little-endian fields and atomic validation before loading.

Provenance: the existing ABI1 DESIGN.md records original DSP and no external third-party implementation or external pinned revision. License: GPL-3.0-only, full text in the root LICENSE; no new third-party source or unverified upstream revision is claimed.
