# Essentials VCF — experimental ABI 2

Offline engine2 port only; stable essentials.vcf identity, GPL-3.0-only. The ABI1 app still uses its original module. DSP: dsp.h; registration: module.cpp. Tests are maintained together in native/tests2/module_tests.cpp (reference, matrix, reset, scalar/no-exception builds) and integration_tests.cpp (graph/voice proof), rather than duplicated per module.

TPT four-pole filter with low12 and low24 outputs, logarithmic cutoff and linear resonance. Per-voice double state retains low-cutoff/high-Q precision. Exact tan coefficients cache per channel until cutoff changes; the original small-state tail rule is retained. Constant-cutoff counter test: four calculations for four channels initially, zero on unchanged next block. Approximate tan is not used.

See [spec](../../../docs/specs/ABI2_SPEC.md) and [review](../../../docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md) for evidence and limitations. No hardware/plugin adapter claim.
