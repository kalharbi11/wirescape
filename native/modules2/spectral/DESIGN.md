<!-- SPDX-License-Identifier: GPL-3.0-only -->
# sound.spectral: design notes

## Provenance
- **Prototype:** the owner's *Conjunctions* web app (`src/dsp.js` and `src/procdefs.js`, as attached to issue #302's builder session on 2026-11-06), processes `morph`, `cross` and `combine` (max/mean/min/diff). PR B adds `pvinter` and `vocoder`. Conjunctions renders whole files offline in a worker.
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

The blend case differs most because the module's phase accumulators start at the first sounding frame rather than at file start.

**Listening note:** I can't listen in this environment. The numbers say the module reproduces the prototype's spectra; the owner should confirm by ear (Morph with a slow LFO, Cross on chant → pad).

## Not done / follow-ups
- Interleave and Vocoder (PR B; their parameters append at slots 9–15 and `mode` widens to 0–7. Plain values are saved, so existing patches are unaffected).
- Engine re-query of `latency_frames()` after a parameter-driven change (T2 engine work, follow-up issue).
- If Pi evidence shows the per-hop burst is too high at 96 kHz/4096, spread the frame over the hop at the cost of one extra hop of latency.
