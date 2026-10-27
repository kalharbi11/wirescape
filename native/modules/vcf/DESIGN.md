# VCF — DSP design

## Algorithm, semantics and provenance

Original 4-stage trapezoidal/TPT first-order low-pass cascade. Each pole uses `g=tan(pi*f/fs)`, `a=g/(1+g)`, output `y=a*x+(1-a)*z`, updated state `z'=2*y-z`. A single four-stage feedback loop is solved algebraically per sample with `k = Resonance*(3.2/0.95)` and `Y4=(a^4*x + Y4_at_zero_input)/(1+k*a^4)`, so feedback is not delayed or iterated. The first input is `x-k*Y4`; after stage two, export `low12`, after stage four, export `low24`. With k=0, stages give unity DC gain and asymptotic -12/-24 dB/oct slopes. With resonance, the 12 dB output is an intermediate tap of the same 4-pole resonant network, not an independently tuned two-pole SVF. The response is not claimed to be Butterworth or an exact analog filter. To avoid numerical faults from massive finite upstream inputs, pre-filter magnitudes above 1e6 are limited at the input; ordinary audio magnitudes are unchanged and filter outputs are not clipped. Pole memory below 1e-20 is set to zero to avoid denormals. Critical resonance/rapid modulation and cutoff near Nyquist remain subject to advanced numerical/audio review.

## Work and state contract

All mutable DSP state is private to the created instance. `create` uses nothrow allocation and `destroy` frees it; `prepare` validates and initializes its fixed state. `process`, `reset` and `event` perform no dynamic allocation, locking, I/O or host callbacks. Work is O(frames) with fixed per-sample operations, no unbounded loops or recursion. Every output writes each requested frame, including null/unpatched inputs. Empty spans do not change state. Events are applied at their caller-supplied sample boundary; sample-position values are not used to conceal block-dependent processing.

## Source and limits

No external third-party DSP implementation, presets, device driver, font or runtime dependency was included. The math and test code are original; the only implementation pattern adopted from the kit is its descriptor/callback ABI and existing GPLv3 `LinearRamp` contract. All Linux offline outcomes are reproducible at the verified toolchain, but neither operating-system realtime guarantees nor listening quality follows from offline assertions. Integration tests on Windows and the actual application, including save/load and audio audition, remain necessary; Raspberry Pi has not been tested.
