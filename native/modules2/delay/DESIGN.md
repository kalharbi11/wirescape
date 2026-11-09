<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Design notes: fx.delay

Provenance: original code. No third-party source, no pinned revisions.

Textbook pieces:
- Fractional delay reads use 4-point, 3rd-order Hermite (Catmull-Rom) interpolation (standard in the DSP literature,
  for example the musicdsp.org "Hermite interpolator" note and Julius O. Smith's *Physical Audio Signal Processing*,
  "Delay-line interpolation").
- Feedback tone: one-pole low-pass, or high-pass as `x - lowpass(x)`, cross-faded in over |tone| < 0.25 so the middle is exactly flat.
- DC blocker: first-order `y[n] = x[n] - x[n-1] + R y[n-1]`, R set for 10 Hz.
- Soft limiter: identity up to 1.0, then `1 + 0.5 tanh((|x|-1)/0.5)` (continuous slope at the knee, ceiling 1.5).

Decisions:
- The limiter sits on the value written to the line (input plus feedback), so line contents are at most 1.5 and the equal-power
  output is at most sqrt(1 + 1.6^2) < 2; the wet read is also clamped to 1.6 as a guard against interpolation overshoot.
- Glide slews at most 0.5 samples per sample (read speed 0.5x..1.5x) on top of a 50 ms one-pole, so big jumps stay click-free.
- Ping-pong sums to mono as an average, so an unpatched `right` (which follows `left`) is not doubled.
