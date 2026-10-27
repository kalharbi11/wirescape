<!-- SPDX-License-Identifier: GPL-3.0-only -->
DSP is in dsp.h, wrapper registration in module.cpp. No Qt/OS/allocating containers or allocation in processing. Fixed16-channel state is prepared once. SDK owns validation; engine owns base smoothing/normalized modulation/taper. Double phase/IIR state is retained where documented for precision/reference matching. No fast tan approximation is used. Voice groups are four-wide; VCA uses SDK SIMD, while transcendental/IIR scalar math is retained pending measured equivalent SIMD implementations.

## Behavior

Cutoff coefficients are computed once per changed value per voice; a varying parameter row is evaluated sample by sample. Host D7 smoothing replaces the ABI1 internal cutoff/resonance parameter ramps. D14 requires finite producer inputs: an out-of-contract NaN triggers SDK output validation/silencing, and resetting restores the IIR state. Infinity is clamped by the current input bound to +/-1e6. This is not ABI1's nonfinite-input-to-zero policy; hosts must supply finite input.

## Provenance

Source: `native/modules/vcf/module.cpp` and the ABI2 port at main `f23ea96`. License: GPL-3.0-only. The existing ABI1 DESIGN.md records original TPT-cascade DSP and no external third-party implementation or external pinned revision.
