<!-- SPDX-License-Identifier: GPL-3.0-only -->
DSP is in dsp.h, wrapper registration in module.cpp. No Qt/OS/allocating containers or allocation in processing. Fixed16-channel state is prepared once. SDK owns validation; engine owns base smoothing/normalized modulation/taper. Double phase/IIR state is retained where documented for precision/reference matching. No fast tan approximation is used. Voice groups are four-wide; VCA uses SDK SIMD, while transcendental/IIR scalar math is retained pending measured equivalent SIMD implementations.

## Behavior

Gate cable precedence is per voice: when the gate signal is connected, all note events are ignored, even when the cable is low. The Schmitt gate (>0.5 on, <0.25 off) triggers/releases envelopes. Without a connected gate, NOTE_ON establishes source/note ownership; only matching NOTE_OFF/CHOKE releases that voice. Stage durations latch on entry. This added cable/event behavior intentionally extends the ABI1 event-only module.

## Provenance

Source: `native/modules/adsr/module.cpp` and the ABI2 port at main `f23ea96`. License: GPL-3.0-only. Original Wirescape code, GPL-3.0-only, no upstream source. There is no ABI1 DESIGN.md; no external source or pinned upstream revision is invented here.
