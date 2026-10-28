<!-- SPDX-License-Identifier: GPL-3.0-only -->
DSP is in dsp.h, wrapper registration in module.cpp. No Qt/OS/allocating containers or allocation in processing. Fixed16-channel state is prepared once. SDK owns validation; engine owns base smoothing/normalized modulation/taper. Double phase/IIR state is retained where documented for precision/reference matching. No fast tan approximation is used. Voice groups are four-wide; VCA uses SDK SIMD, while transcendental/IIR scalar math is retained pending measured equivalent SIMD implementations.

## Behavior

The callback multiplies audio * CV * the supplied plain gain row, including a varying gain row, in that order. CONSTANT is valid only when all three sources are constant; missing CV is unity. ABI2 uses the host D7 smoothing pipeline and does not add ABI1's internal plain-unit gain ramp to each callback row. A direct callback parameter step therefore applies immediately; the host supplies smoothed rows for live base changes.

## Provenance

Source: `native/modules/vca/module.cpp` and the ABI2 port at main `f23ea96`. License: GPL-3.0-only. Original Wirescape code, GPL-3.0-only, no upstream source. There is no ABI1 DESIGN.md; no external source or pinned upstream revision is invented here.
