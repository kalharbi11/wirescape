<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Design and provenance

Original Wirescape code, GPL-3.0-only, no upstream source. No Surge or SST DSP is
included. The CHAOS decomposition inventory informs the behaviour vocabulary only.
The source example structure follows the MIT SDK random-source example.

`comb_body.h`, `coupler.h` and `drift.h` are deliberately independent components
for later M7 decomposition. Delays use linear interpolation; the body uses three
pitch-related ratios (1, 1.4983, 2.003) and saturating feedback. The stereo A/B pan
law is linear, with independent left/right resonator and coupler histories. A
one-pole post filter, DC blocker, rational soft clip and −1 dBFS ceiling bound output.

All comb and coupler storage is requested with WS2_LARGE in prepare. The memory
query conservatively includes alignment padding; allocation and destruction belong
to the host. At 48 kHz eight prepared voices reserve approximately 0.60 MB plus
host/wrapper storage. Process and reset allocate nothing. Supported frame counts
are 1..prepared maximum, rates 44.1/48/96 kHz, channels 1..16 with only the first
eight active. Parameters are read per sample, respecting host modulation.

State is versioned little-endian words/floats, staged and validated before commit.
Only generator/instability state is serialized. Delay lines and filter memory are
cleared on load; this intentional patch-state policy means no continuation-equivalence
claim for resonator tails. Reset determinism and state-byte restoration are tested.
No constant flag is set on running audio, including silent capped channels.
