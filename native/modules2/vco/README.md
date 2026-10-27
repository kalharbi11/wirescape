# Essentials VCO — experimental ABI 2

Offline engine2 port only; stable essentials.vco identity, GPL-3.0-only. The ABI1 app still uses its original module. DSP: dsp.h; registration: module.cpp. Tests are maintained together in native/tests2/module_tests.cpp (reference, matrix, reset, scalar/no-exception builds) and integration_tests.cpp (graph/voice proof), rather than duplicated per module.

Pitch input is 1 V/oct: 0=C4 at the default Frequency220 setting. Frequency acts as a tuning ratio Frequency/220 when pitch is connected. Unconnected pitch uses last NOTE_ON frequency, otherwise Frequency Hz. FM is additive Hz, clamped 0..0.49*sample_rate. Pitch cable wins over NOTE_ON frequency; CHOKE resets channel phase and event frequency. NOTE_OFF does not silence the oscillator; gate an ADSR/VCA. Double phase retains reference precision.

See [spec](../../../docs/specs/ABI2_SPEC.md) and [review](../../../docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md) for evidence and limitations. No hardware/plugin adapter claim.
