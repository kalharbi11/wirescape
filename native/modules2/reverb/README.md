<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Reverb (`fx.reverb`)

Original stereo **8-line feedback delay network** (FDN) reverb for rooms through halls. Two allpass stages per channel diffuse the input, the feedback matrix is orthonormal, and the wet outputs use orthogonal projections. Audio and feedback history are processed continuously even at Mix 0.

## Ports

| ID | Direction | Behaviour |
|---|---|---|
| `left` | input | Audio, `WS2_SUM` (sums poly cable), stereo pair |
| `right` | input | Audio, `WS2_SUM`; when unconnected reads `left` |
| `left` | output | Left audio, one voice, stereo pair |
| `right` | output | Right audio, one voice, stereo pair |

## Parameters

| Slot / ID | Range (default) | Meaning |
|---|---|---|
| 0 `size` | 0–1 (0.5) | Prime line lengths between 0.25× and 1× base; interpolated read-position glide (~50 ms) |
| 1 `decay` | 0.2–20 s (2.5) | RT60, logarithmic; feedback compensates for line length |
| 2 `damping` | 0–1 (0.4) | High-frequency feedback attenuation; 0 bright, 1 very dark |
| 3 `predelay` | 0–0.25 s (0.02) | Pre-delay, smoothly adjusted |
| 4 `width` | 0–1 (1) | 0 mono wet; 1 maximally decorrelated |
| 5 `motion` | 0–1 (0.2) | Deterministic slow line-length modulation |
| 6 `freeze` | 0 / 1 (0) | Stepped: 20-ms crossfade to mute new input, remove damping and set unity feedback |
| 7 `mix` | 0–1 (0.3) | Equal-power crossfade; 0 is bit-exact dry passthrough |

The outputs never claim `CONSTANT` while the network is running. No reported latency. `reset` and patch-state load clear all audio history; no audio or RNG data is serialized (parameters are owned by the host).

Memory is allocated only in `prepare` using `WS2_LARGE`, proportionate to sample rate up to the 96 kHz target. No third-party DSP or platform dependencies. Stereo inputs are summed by the host; output has one channel, not one per stereo side.
