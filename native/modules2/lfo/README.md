<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Essentials LFO — experimental ABI 2

Offline engine2 port only; stable essentials.lfo identity, GPL-3.0-only. The ABI1 app still uses its original module. DSP: dsp.h; registration: module.cpp. Tests are maintained together in native/tests2/module_tests.cpp (reference, matrix, reset, scalar/no-exception builds) and integration_tests.cpp (graph/voice proof), rather than duplicated per module.

Sine control oscillator, rate/depth/offset retain ABI1 meanings; sync/beats are new parameters.

Outputs (both polyphonic signals, with stable indices): `control` (index 0, bipolar sine with depth and offset), and the **appended** `phase` (index 1, unipolar 0..1 ramp). `phase` is the same per-voice oscillator phase used for the sine sample, normalised by 2π **before** the free-run step. It is not scaled by depth or offset, so it advances even at depth 0. The existing output and all parameter IDs/indices are unchanged; patches saved without `phase` still load and render as before. Sync uses transport quarter-note position. When transport is missing, manual rate runs. Stopped sync holds phase; locate sets it directly. Double phase retains low-rate precision.

See [spec](../../../docs/specs/ABI2_SPEC.md) and [review](../../../docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md) for evidence and limitations. No hardware/plugin adapter claim.
