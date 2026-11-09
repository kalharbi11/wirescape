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
| 6 **Interleave** | Alternates chunks of whole frames: **Frames of A**, then **Frames of B** (B scaled by **B level**). With **Phase vocoder** the output phase runs on through each switch, so partials smear and glide; **Source phase** just switches, cleaner and closer to a splice |
| 7 **Vocoder** | A is the modulator, B the carrier. A's energy in **Bands** log-spaced bands (70 Hz to 12 kHz) is imposed on B. **Flatten carrier** first removes B's own spectral tilt, **Release** sets how fast the bands close, and **Noise** adds hiss in the upper bands so sibilants survive a tonal carrier. A rich, sustained carrier (saw, pad, noise) works best |

## Ports
| ID | Direction | Notes |
|---|---|---|
| `a` | in | Audio. Polyphonic cables are **summed** (SUM). Unpatched = silence |
| `b` | in | Audio, summed. Unpatched = silence |
| `audio` | out | One voice |

## Parameters
| Slot | ID | Range (default) | Notes |
|---|---|---|---|
| 0 | `mode` | 0–7 (0 Morph) | Stepped; a CV crossfades to the rounded mode over four frames |
| 1 | `size` | 0–3 → 512 / 1024 / **2048** / 4096 | FFT size at 44.1/48 kHz; doubled at 88.2/96 kHz, ×4 above 132 kHz. Not modulatable |
| 2 | `morph` | 0–1 (0.5) | Morph position: 0 = A, 1 = B |
| 3 | `curve` | 0 Geometric / 1 Linear | Morph |
| 4 | `pitch` | 0 Louder bin / 1 Blend | Morph |
| 5 | `amount` | 0–1 (1) | Cross: how much of A's envelope. 0 = B unchanged |
| 6 | `smooth` | 0–1 (0.15) | Cross: envelope smoothing |
| 7 | `b_level` | 0–2 (1) | B's weight in Max / Mean / Min / A − B, B's level in Interleave |
| 8 | `level` | 0–2 (1) | Output level, applied per sample |
| 9 | `chunk_a` | 1–64 (5) | Interleave: frames of A per chunk. Stepped, modulatable |
| 10 | `chunk_b` | 1–64 (5) | Interleave: frames of B per chunk. Stepped, modulatable |
| 11 | `phase` | 0 Phase vocoder / 1 Source phase | Interleave |
| 12 | `bands` | 6–64 (24) | Vocoder band count. Stepped, modulatable. Few bands sound robotic; many approach cross-synthesis |
| 13 | `flatten` | 0–1 (0.85) | Vocoder: how much of B's own envelope is divided out |
| 14 | `release` | 0.005–0.4 s (0.05), log | Vocoder band release (attack is 4 ms) |
| 15 | `noise` | 0–1 (0.2) | Vocoder: hiss above about 3 kHz, following A |

Controls other than `level` are read once per hop (every N/4 samples); the overlap-add smooths the steps. The interleave schedule runs on the frame clock in every mode, so a CV switch into Interleave lands mid-cycle rather than restarting it.

## Patch-guide limitations
Modes 6 (Interleave) and 7 (Vocoder), and parameter slots 9–15, are implemented
on draft PR #310 but are **not available on `main` until that PR merges**. A
patch that uses either mode requires a build containing PR B; do not present
such patches as compatible with older builds supporting only modes 0–5.
Existing modes 0–5 and v1 state remain compatible with the new implementation.

Interleave switches whole STFT frames on the N/4 hop clock, not at arbitrary
sample positions. Phase-vocoder phase continuity may smear sharp attacks;
Source phase switches more directly. State v2 restores the chunk schedule, not
input rings or overlap-add audio history, so an in-progress sound must be
reanalyzed after loading. Target-device CPU and subjective audio suitability
still require the three task-specific Pi probes and owner listening; hosted
CI alone is not that evidence.

## Latency
`latency_frames()` returns the current FFT size N (for example 2048 frames = 42.7 ms at 48 kHz). The engine reads it when it builds the plan, before any parameter arrives, so it shows the default size's latency until the next replan after a `size` change (Keel, #302). Changing `size` lets the old frames fade out (0.75 N), then the new size fades in.

## State
Saves the schedule only (version 2): hop phase, current/previous mode and crossfade position, size and a pending size change, the RNG and the interleave position. Version 1 states (before Interleave) still load, at interleave position 0. Audio history (input rings, the overlap-add buffer, per-bin phase) isn't saved; after a load the analysis restarts on the saved hop grid.

## CPU
Every N/4 samples: two forward FFTs and one inverse, plus O(N) per-bin work (a second render during a mode crossfade). Silent input skips the work, and the output goes CONSTANT once the tail has drained. Check: `python tools/module_cpu_probe.py sound.spectral`.
