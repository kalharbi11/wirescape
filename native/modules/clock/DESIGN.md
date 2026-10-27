# Clock — DSP design

## Algorithm, semantics and provenance

Sample-phase accumulator `phase` in [0,1) advances after each running output by `(BPM/60 * integerTicks) / sampleRate`, wrapping by bounded subtraction of floor(phase). A running phase of zero emits 1; the high window ends at Duty; a tiny `1e-12` phase-boundary tolerance removes false extra high samples from accumulated double rounding at exact boundaries. BPM uses the supplied 5 ms LinearRamp, while ticks/duty/run/reset are immediate. The incoming reset rising edge sets phase zero before writing that sample; held-high input does not repeat reset. Digital periods/duty are quantized to sample boundaries and rates above Nyquist alias; this is not host transport scheduling.

## Work and state contract

All mutable DSP state is private to the created instance. `create` uses nothrow allocation and `destroy` frees it; `prepare` validates and initializes its fixed state. `process`, `reset` and `event` perform no dynamic allocation, locking, I/O or host callbacks. Work is O(frames) with fixed per-sample operations, no unbounded loops or recursion. Every output writes each requested frame, including null/unpatched inputs. Empty spans do not change state. Events are applied at their caller-supplied sample boundary; sample-position values are not used to conceal block-dependent processing.

## Source and limits

No external third-party DSP implementation, presets, device driver, font or runtime dependency was included. The math and test code are original; the only implementation pattern adopted from the kit is its descriptor/callback ABI and existing GPLv3 `LinearRamp` contract. All Linux offline outcomes are reproducible at the verified toolchain, but neither operating-system realtime guarantees nor listening quality follows from offline assertions. Integration tests on Windows and the actual application, including save/load and audio audition, remain necessary; Raspberry Pi has not been tested.
