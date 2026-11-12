<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Quad VCA / low-pass gate

Module ID: `essentials.quad_vca` (ABI 2, version 1). Original four-channel, polyphonic processor. Each channel can be a linear VCA or a two-pole, vactrol-like low-pass gate (LPG).

| Port | Hint | Poly | Unpatched |
|---|---|---|---|
| `in1` | audio | 1–16 voices | silence |
| `in2`…`in4` | audio | 1–16 voices | preceding channel's **input**, not output |
| `cv1`…`cv4` | unipolar CV | 1–16 voices | 1 (knob only) |
| `out1`…`out4` | audio | 1–16 voices | four independent outputs |
| `mix` | audio | 1–16 voices | sum of four outputs, clamped to ±4 (not normalized) |

| Parameter | Range | Default | Meaning |
|---|---|---|---|
| `level1`…`level4` | 0–1 | 1 | per-voice, modulatable gain; multiplied by `cvN` |
| `mode1`…`mode4` | 0 or 1 | 0 | 0 = VCA, 1 = LPG |
| `response` | 0–1 | 0.4 | LPG release, exponential from 60 ms to 1.5 s |

VCA output = input × clamp(level × CV, 0, 1). LPG: a 2 ms attack/60–1500 ms release follower controls amplitude and the cutoff of a two-pole low-pass filter, from about 40 Hz to min(18 kHz, 0.45 × sample rate), with Q about 0.6. A single source at `in1` fans out to all four channels; connect four distinct envelopes to `cv1`…`cv4` to animate them independently. The host sets the voice count from the widest patched input (maximum 16); mono signals broadcast. Reset clears all follower/filter state. No state is saved beyond the patch's parameters.
