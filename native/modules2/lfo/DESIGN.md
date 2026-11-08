<!-- SPDX-License-Identifier: GPL-3.0-only -->
DSP is in dsp.h, wrapper registration in module.cpp. No Qt/OS/allocating containers or allocation in processing. Fixed16-channel state is prepared once. SDK owns validation; engine owns base smoothing/normalized modulation/taper. Double phase/IIR state is retained where documented for precision/reference matching. No fast tan approximation is used. Voice groups are four-wide; VCA uses SDK SIMD, while transcendental/IIR scalar math is retained pending measured equivalent SIMD implementations.

## Behavior

Tempo sync defaults off: transport presence alone does not change free-running rate. Sync enabled without transport falls back to free running. With transport, beat position and Beats/cycle determine phase; stopped transport holds phase and a locate changes it. Beats/cycle is a continuous global D7-modulatable parameter; the stepped Sync switch is not CV-modulated. These transport controls extend the ABI1 free-running oscillator.

The appended `phase` signal output (index 1, `WS2_CV_UNI`) publishes `phase[c]/tau` at each sample, clamped to the largest float below 1 to avoid rounding a near-wrap phase to 1. Sync reads the transport-derived phase, including locate and stopped position; missing transport uses free-run. Both outputs set the host-selected voice count and remain varying. `control` (index 0) retains its original calculation and phase-advance order unchanged. The host-side telemetry mapping is a separate B-wire task (#359), not part of this module.

## Provenance

Source: `native/modules/lfo/module.cpp` and the ABI2 port at main `f23ea96`. License: GPL-3.0-only. Original Wirescape code, GPL-3.0-only, no upstream source. There is no ABI1 DESIGN.md; no external source or pinned upstream revision is invented here.
