<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Spectral (`sound.spectral`)

Joins two sources, **A** and **B**, in the frequency domain. One streaming STFT engine (Hann windows, hop N/4) is shared by every mode. Switching modes crossfades over four frames, so it never clicks, and `mode` can be driven by a CV.

| Mode | What it does |
|---|---|
| 0 **Morph** (default) | Each bin's magnitude moves from A to B, geometrically (meets in the middle in dB) or linearly. Its frequency comes from the louder source's bin, or from a blend of both. Partials bend instead of crossfading. Patch an LFO, envelope or slew into **Morph** for motion |
| 1 **Cross** | B's bins, reshaped by A's spectral envelope (`(A/B)^amount`). **Smoothing** blurs both envelopes, sliding from full cross-synthesis towards envelope transfer, so B's pitch comes through |
| 2 **Max** | The louder bin of A or B (B weighted by **B level**) |
| 3 **Mean** | The average magnitude, with the louder bin's phase |
| 4 **Min** | The quieter magnitude, with A's phase: only what both share |
| 5 **A − B** | A's magnitude minus B's, floored at 0: carves B out of A |

## Ports
| ID | Direction | Notes |
|---|---|---|
| `a` | in | Audio. Polyphonic cables are **summed** (SUM). Unpatched = silence |
| `b` | in | Audio, summed. Unpatched = silence |
| `audio` | out | One voice |

## Parameters
| Slot | ID | Range (default) | Notes |
|---|---|---|---|
| 0 | `mode` | 0–5 (0 Morph) | Stepped; a CV crossfades to the rounded mode over four frames |
| 1 | `size` | 0–3 → 512 / 1024 / **2048** / 4096 | FFT size at 44.1/48 kHz; doubled at 88.2/96 kHz, ×4 above 132 kHz. Not modulatable |
| 2 | `morph` | 0–1 (0.5) | Morph position: 0 = A, 1 = B |
| 3 | `curve` | 0 Geometric / 1 Linear | Morph |
| 4 | `pitch` | 0 Louder bin / 1 Blend | Morph |
| 5 | `amount` | 0–1 (1) | Cross: how much of A's envelope. 0 = B unchanged |
| 6 | `smooth` | 0–1 (0.15) | Cross: envelope smoothing |
| 7 | `b_level` | 0–2 (1) | B's weight in Max / Mean / Min / A − B |
| 8 | `level` | 0–2 (1) | Output level, applied per sample |

Controls other than `level` are read once per hop (every N/4 samples); the overlap-add smooths the steps.

## Latency
`latency_frames()` returns the current FFT size N (for example 2048 frames = 42.7 ms at 48 kHz). The engine reads it when it builds the plan, before any parameter arrives, so it shows the default size's latency until the next replan after a `size` change (Keel, #302). Changing `size` lets the old frames fade out (0.75 N), then the new size fades in.

## State
Saves the schedule only: hop phase, current/previous mode and crossfade position, size and a pending size change, and the RNG. Audio history (input rings, the overlap-add buffer, per-bin phase) isn't saved; after a load the analysis restarts on the saved hop grid.

## CPU
Every N/4 samples: two forward FFTs and one inverse, plus O(N) per-bin work (a second render during a mode crossfade). Silent input skips the work, and the output goes CONSTANT once the tail has drained. Check: `python tools/module_cpu_probe.py sound.spectral`.
