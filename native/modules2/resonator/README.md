<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Resonator (`essentials.resonator`)

Polyphonic, three-tap comb resonator adapted from CHAOS-lite. Strike to play a
deterministic five-millisecond mallet/pick, or patch `audio` to use an external
exciter. `pitch` is 1 V/oct relative to C4 (261.6256 Hz at 0), and a mono
input broadcasts across voices. Only voices 1–8 ring; voices 9–16 are always
zero. The processor has one poly `audio` output, and no note gate is needed.

| Port | Direction | Purpose |
|---|---|---|
| `audio` | in, poly | External exciter; dry source for Mix |
| `pitch` | in, poly | Pitch 0 = C4, unpatched 0 |
| `strike` | in, poly | Rising edge (>0.5; release <0.25) triggers one burst |
| `audio` | out, poly | Damped resonator (first eight voices) |

| Parameter | Range / default | Description |
|---|---|---|
| `tune` | −24…24 st / 0 | Semitones added to the pitch cable |
| `structure` | 0…3 / 0 | Body, Harmonic, Bar, Bell |
| `decay` | 0.05…20 s / 1.5 s, log | Fundamental T60 (−60 dB) |
| `damping` | 0…1 / 0.3 | Loss of high-frequency content per pass |
| `strike_tone` | 0…1 / 0.5 | Soft mallet to bright pick |
| `strike_level` | 0…1 / 0.8 | Internal strike excitation amplitude |
| `mix` | 0…1 / 1 | Dry input to wet body |
| `level` | 0…1 / 0.7 | Output linear level |

A pitch-tracking four-pole wet lowpass favors the fundamental without
attenuating the comb feedback itself. The four structures have tap frequency ratios Body (1, 1.4983, 2.003),
Harmonic (1, 2, 3), Bar (1, 2.756, 5.404), Bell (1, 2.32, 4.25).
Tuning clamps to 20 Hz at 96 kHz and to 0.45 times sample rate at the
top end. Pitch and structure changes glide over approximately 12 ms.

Reset re-seeds independent voice exciters and clears comb/filter memory.
Patch state records **no** sounding history; loading clears the tails, so
saved patches restore control values rather than continuing old strikes.
No realtime allocations or device I/O. The hard output ceiling is ±2.
