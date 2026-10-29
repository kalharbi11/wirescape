<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS-lite

An original stereo instrument: two morphing saw/square/sine oscillators, coupling,
three damped combs per side, seeded instability, roughness AM and self-modulated LFO.
It is not a Surge port and does not reproduce the legacy CHAOS sound.

| Port | Meaning |
|---|---|
| `pitch` | Poly, 1/oct, C4 = 0; frequency limited to 16 Hz–0.45 × rate |
| `gate` | Poly gate, unpatched 1; drives amplitude and restarts pitch fall |
| `velocity` | Poly 0..1 amplitude, unpatched 1 |
| `fm` | Poly bipolar pitch CV, 0.25 oct/unit |
| `left`, `right` | Stereo pair `stereo`; channels represent voices |

Prepared voices are capped at 8. If the host provides 9–16 channels, those channels
are explicitly silent. This processor follows its input channel count. Oscillators
are intentionally simple (not band-limited); high-pitched saw/square tones can alias.
Gate amplitude is immediate; patch an envelope externally when a release is desired.

Parameter slots are stable and listed in `module.json`; ranges/defaults are in the
SDK descriptor in `dsp.h`. Shapes span saw 0 → square 1 → sine 2. A/B octave is −3..3,
B detune −60..60 cents, A/B level 0..1, pan −1..1. Route is 0 parallel, 1 ring/AM,
2 delay-bend (3.5 ± 2.5 ms). Depth, mix, roughness, drift, jitter, fall and LFO depth
are 0..1; body tune −2..2 octaves; body feedback 0..0.92, damping 0..0.99.
Roughness rate is 1..150 Hz; LFO rate 0.02..12 Hz, self-mod 0..1 and target
0 A pitch / 1 body tune / 2 cutoff. Cutoff 20..20,000 Hz; final level 0..1.

Named starting points (all unspecified values retain descriptor defaults):

| Name | Parameter values |
|---|---|
| Warm fifth | shape_a=2, shape_b=2, detune=7, body_mix=0.15, roughness=0 |
| Bent wire | route=2, depth=0.8, body_mix=0.65, feedback=0.7, drift=0.25 |
| Broken bell | route=1, depth=0.9, shape_a=2, octave_b=1, body_tune=1, fall=0.1 |
| Slow weather | lfo_target=1, lfo_rate=0.1, lfo_depth=0.5, drift=0.8, jitter=0.2 |

Reset is deterministic from the patch seed and channel. State v1 preserves oscillator
and LFO phases, drift/fall and random state; loading clears comb/delay/filter history
instead of storing megabytes of DSP buffers. This is patch restoration, not seamless
audio continuation. Unknown versions are preserved by the host; malformed state is
rejected without changing the current state. Jitter advances on an internal fixed
128-frame clock, so cutting a host block does not change the random sequence.

`abi2_chaos_lite_tests --benchmark` measures a real engine graph with two instances ×
four voices, 48 kHz/128, 10,000 timed blocks after warmup, without opening a device.
