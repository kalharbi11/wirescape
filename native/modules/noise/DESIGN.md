# Noise — DSP design

## Algorithm, semantics and provenance

Original xorshift32 PRNG, with unsigned 32-bit state seeded `0x12345678`. Each sample applies x^=x<<13; x^=x>>17; x^=x<<5. Upper 24 bits are converted by `bipolar = (state >> 8) / 8388608 - 1`. This deliberately never accesses OS randomness, system clocks or files; each instance has private state. Draws continue even when Level is zero. `wirescape::LinearRamp` gives a sample-counted 5 ms gain transition (initial target/reset snap). There are no feedback or denormal states. This is uniform pseudo-random white noise, not cryptographic randomness; its sequence is fixed for reproducible offline renders.

## Work and state contract

All mutable DSP state is private to the created instance. `create` uses nothrow allocation and `destroy` frees it; `prepare` validates and initializes its fixed state. `process`, `reset` and `event` perform no dynamic allocation, locking, I/O or host callbacks. Work is O(frames) with fixed per-sample operations, no unbounded loops or recursion. Every output writes each requested frame, including null/unpatched inputs. Empty spans do not change state. Events are applied at their caller-supplied sample boundary; sample-position values are not used to conceal block-dependent processing.

## Source and limits

No external third-party DSP implementation, presets, device driver, font or runtime dependency was included. The math and test code are original; the only implementation pattern adopted from the kit is its descriptor/callback ABI and existing GPLv3 `LinearRamp` contract. All Linux offline outcomes are reproducible at the verified toolchain, but neither operating-system realtime guarantees nor listening quality follows from offline assertions. Integration tests on Windows and the actual application, including save/load and audio audition, remain necessary; Raspberry Pi has not been tested.
