<!-- SPDX-License-Identifier: GPL-3.0-only -->
DSP is in dsp.h, wrapper registration in module.cpp. No Qt/OS/allocating containers or allocation in processing. Fixed16-channel state is prepared once. SDK owns validation; engine owns base smoothing/normalized modulation/taper. Double phase/IIR state is retained where documented for precision/reference matching. No fast tan approximation is used. Voice groups are four-wide; VCA uses SDK SIMD, while transcendental/IIR scalar math is retained pending measured equivalent SIMD implementations.

## Behavior

NOTE_OFF retains the held note pitch; amplitude gating belongs to an envelope. CHOKE clears held pitch and resets phase, returning to the frequency parameter. A pitch cable takes precedence over held notes. The frequency sum including FM is clamped to [0, 0.49*sample_rate]: negative frequency holds phase rather than reversing it. The tests pin these ABI2 callback semantics.

## Provenance

Source: `native/modules/vco/module.cpp` and the ABI2 port at main `f23ea96`. License: GPL-3.0-only. Original Wirescape code, GPL-3.0-only, no upstream source. There is no ABI1 DESIGN.md; no external source or pinned upstream revision is invented here.
