<!-- SPDX-License-Identifier: GPL-3.0-only -->
# sound.spectral: design notes

## Provenance
- **Prototype:** the owner's *Conjunctions* web app (`src/dsp.js` and `src/procdefs.js`, as attached to issue #302's builder session on 2026-11-06), processes `morph`, `cross`, `combine` (max/mean/min/diff) (#302, PR #304), then `pvinter` and `vocoder` (#305). Conjunctions renders whole files offline in a worker.
- **Code:** original. The FFT (`fft.h`) is written from scratch: an N/2 complex radix-2 transform with the standard real-split pass. One twiddle table for the largest size serves every smaller size by striding. No third-party code. CDP is named in the prototype only as inspiration; no CDP code was read or copied.
- Licence: GPL-3.0-only.

## What changed from the prototype, and why
| Prototype | Here | Why |
|---|---|---|
| Whole-file analysis, then synthesis | Streaming STFT: input rings, a frame every N/4 samples on a counter of samples processed (never the call boundaries), overlap-add, latency N | Live module; block-size invariant (frame-matrix test) |
| Frames centred on t (N/2 look-ahead) | Frames end at t: input sample s comes out at s + N | No future in a stream |
| `levelise` sources and `finish` peak-normalise the result | Nothing normalises. Spectra are scaled so a full-scale sine is about 1 in its bin, and the plain paths are unity gain (Max with B silent returns A exactly) | No end of file exists |
| Magnitudes in raw FFT units (epsilons and the `1e3` clamp depend on N) | Normalised units: epsilons are amplitude-sized. Cross clamps the gain ratio (≤ 1e3), not the magnitude | Same sound at every size and rate |
| Overlap 2/4/8 (interleave only), otherwise 4 | Fixed 4 | Hann analysis × Hann synthesis only sums to a constant at overlap ≥ 4 (the 2× setting ripples). 8× doubles the CPU for no audible gain in these modes. One hop also keeps every mode on one schedule |
| FFT size per process (256–8192) at 44.1 kHz | 512–4096 at 44.1/48 kHz, ×2 at 88.2/96 kHz | Frequency resolution and time smear sound the same at every rate; 8192 at 48 kHz is too much latency live |
| Morph motion over the render (sweep/back/swell) | Dropped: **Morph is a modulatable parameter**; patch an LFO, envelope or slew | Render-length motions don't exist live (Compass, #302) |
| Morph phase via `atan2` + unwrapping + `cos`/`sin` | Louder-bin pitch: unit-phasor product (exact equivalent: unwrapping only matters when two advances are blended). Blend pitch keeps the unwrapping | Halves Morph's CPU (142 → 75 µs per 2048 frame on x86) |
| Separate process cards | One engine, modes as a stepped, modulatable parameter, with a four-frame spectral crossfade on every mode change (CV included) | Owner's direction: one FFT module. Clickless |
| Combine "provenance" for the scope tint, spectrogram | Dropped | Display is Phase 3A telemetry |
| Each process tracks its own phases | One pass per frame (`track()`) computes magnitudes and each source's phase rotation as a unit phasor for every mode; Morph and Interleave keep their own output phasors | Modes that crossfade can't fight over shared phase state; the magnitudes are reused by Cross and the combine modes |
| Interleave chunk counter is the frame index from file start | Runs on the module's frame clock, saved in state | No file start live; frame-matrix and state tests |
| Interleave overlap 2/4/8 | Fixed 4 (see above) | |
| Vocoder band layout rebuilt with a logarithm per bin whenever the band count changes | Each bin's log-axis position is computed once in `prepare` for the largest FFT, and every size reads it by stride; a `bands` change costs one multiply per bin plus one `exp` and two binary searches per band (review on #310) | `bands` is modulatable, so the change can happen on any hop |
| Vocoder: each bin goes to its nearest band; empty low bands at small N | Triangular bands in band-index units, half-width at least one bin spacing, so every band holds ≥ 2 bins | At N = 512 the 70–100 Hz bands had no bins, so gain 0 and holes in the spectrum |
| Vocoder smoothing per hop (B × 0.6 per hop, attack `exp(-hop/4 ms)`) | Time constants: attack 4 ms, carrier 11.4 ms (the prototype's 0.6 per hop at N = 1024, 44.1 kHz), release from the control | Same envelope speed at every size and rate |
| Vocoder gain in raw FFT units | Normalised units (see above) | The prototype's output level scaled with N^(1 − flatten), +7.2 dB at 1024; `finish()` hid it |
| `Math.random` hiss with a random phase per bin | `ws::Random` (seeded per instance, saved), two bipolar draws per bin, no trig | Reproducible; frame-matrix bit identity includes the hiss |

**Per-bin math:** Morph's geometric magnitude, Morph's blended pitch and Cross's ratio power use `approx.h` (original float log2/exp2/atan2/sincos, accuracy checked against `<cmath>` in the tests: log2 and exp2 within 2e-5 relative, atan2 within 2e-5 rad, sin/cos within 4e-6). Blend splits each bin's advance into the centre's exact quarter turn (pi/2 per bin per hop at overlap 4) plus a small deviation, so the trig never sees large angles. Together these took Morph from about 350 to 200 µs per 4096-point frame on x86.

**Silent-frame rule:** when both analysis windows are exactly zero, the frame is skipped and the phase state resets, in every code path. This makes silence cheap and deterministic. The output is marked CONSTANT only when both inputs are constant and the whole block came out silent.

**Size change:** frames of two sizes can't be crossfaded spectrally. The engine stops framing, the old frames' overlap-add tail fades the output out over 0.75 N, then the new size starts and fades in. With nothing sounding (`live_ == 0`), the switch is immediate.

## Offline parity with the prototype (local check, not CI; Compass, #302)
Inputs: the prototype's own chant (`mode1`, bass, fifth organum, 3 voices) as A and its strings pad (`dread`, 2 s chords) as B, 6 s at 48 kHz, both `levelise`d. Each prototype process ran without `finish`, and the module ran on the same samples with the latency removed. Features: 8 octave-band energy envelopes (62.5 Hz–16 kHz, 2048-point frames, hop 1024) and the spectral centroid.

| Case | Band-envelope correlation | Mean band difference | Median centroid proto / module | Level difference |
|---|---|---|---|---|
| Morph 0.5, geometric, louder bin | 0.987 | 0.06 dB | 371 / 371 Hz | 0.00 dB |
| Morph 0.3, linear, blend | 0.994 | 1.25 dB | 403 / 397 Hz | −0.14 dB |
| Cross, amount 1, smoothing 0.15 | 0.999 | 0.01 dB | 392 / 392 Hz | 0.00 dB |
| Max / Mean / Min / A − B | 1.000 | 0.00 dB | 410 / 391 / 343 / 443 Hz (both) | 0.00 dB |
| Interleave 5/5, source phase, 1024 (schedule aligned) | 1.000 | 0.00 dB | 405 / 405 Hz | 0.00 dB |
| Interleave 5/5, phase vocoder, 1024 (schedule aligned) | 0.997 | 0.79 dB | 409 / 409 Hz | +0.06 dB |
| Vocoder 24 bands, flatten 0.85, release 50 ms, noise 0.2, 1024 | 1.000 | 6.88 dB (level only) | 444 / 417 Hz | −7.03 dB |

The blend case differs because the module's phase accumulators start at the first sounding frame rather than at file start.

Interleave: the prototype centres frame f on f·hop, while the module's frame ends there, so the same chunk pattern sits 2 frames later. With the schedule aligned through `load_state` (chunk position cycle − 2), source phase matches exactly; unaligned the correlation is 0.936, because a live module's schedule phase is arbitrary anyway. Vocoder: the band shapes match (correlation 1.000); the −7 dB is the prototype's FFT-size-dependent level described above. The centroid is lower because of the triangular bands.

**Listening note:** I can't listen in this environment. The numbers say the module reproduces the prototype's spectra; the owner should confirm by ear (Morph with a slow LFO, Cross on chant → pad, Interleave 5/5 PV on pad → saw, Vocoder with a voice or chant into A and a saw or noise into B).

## Not done / follow-ups
- Engine re-query of `latency_frames()` after a parameter-driven change (T2 engine work, follow-up issue).
- If Pi evidence shows the per-hop burst is too high at 96 kHz/4096, spread the frame over the hop at the cost of one extra hop of latency.
