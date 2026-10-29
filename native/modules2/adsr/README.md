<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Essentials ADSR — experimental ABI 2

Offline engine2 port only; stable essentials.adsr identity, GPL-3.0-only. The ABI1 app still uses its original module. DSP: dsp.h; registration: module.cpp. Tests are maintained together in native/tests2/module_tests.cpp (reference, matrix, reset, scalar/no-exception builds) and integration_tests.cpp (graph/voice proof), rather than duplicated per module.

A connected nonzero-channel gate cable wins over all note events. Gates use >0.5 on / <0.25 off. With no cable, NOTE_ON/OFF track source_id/note_id per explicit voice channel; matching CHOKE clears even a release tail. Attack/decay/release times latch at segment start; sustain is sampled. No local held-note fallback: use the shared upstream allocator. Float envelope level and double/sample-count timing retain ABI1 reference behavior.

See [spec](../../../docs/specs/ABI2_SPEC.md) and [review](../../../docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md) for evidence and limitations. No hardware/plugin adapter claim.
