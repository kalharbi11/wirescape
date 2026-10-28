<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Essentials VCA — experimental ABI 2

Offline engine2 port only; stable essentials.vca identity, GPL-3.0-only. The ABI1 app still uses its original module. DSP: dsp.h; registration: module.cpp. Tests are maintained together in native/tests2/module_tests.cpp (reference, matrix, reset, scalar/no-exception builds) and integration_tests.cpp (graph/voice proof), rather than duplicated per module.

Audio * gain_cv * Gain, without clipping. Unpatched gain_cv=1. Mono values broadcast to voices; output CONSTANT only when all factors are constant. f4 processes four voice lanes with scalar fallback/tails. The host smooths normalized base Gain over5ms; multiplication uses float values, so automated transitions may round differently from ABI1 double ramp multiplication.

See [spec](../../../docs/specs/ABI2_SPEC.md) and [review](../../../docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md) for evidence and limitations. No hardware/plugin adapter claim.
