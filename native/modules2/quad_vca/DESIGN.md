<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Quad VCA design and provenance

Original DSP written for Wirescape, 2026-11-11. No ported third-party code, dependencies or external DSP assets.

Four independent per-voice gain cells share one Response control. VCA is exactly linear; LPG uses a one-pole attack/release follower and a two-integrator TPT state-variable low-pass with fixed Q≈0.6. The cutoff follows the same envelope as the amplitude. Cutoff and release coefficient lookup tables are prepared off the audio thread at the configured sample rate; audio processing uses only fixed-size arrays and bounded interpolation. The low-pass state is cleared when returning to VCA. The mix sums four post-gate outputs and clamps at ±4, without normalization.

Task/IDs: `docs/tasks/QUAD_VCA_LPG_490.md`, Q1 #601. No ABI, engine, protocol or existing VCA changes.
