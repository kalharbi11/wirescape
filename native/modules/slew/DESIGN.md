# Slew — DSP design

## Algorithm, semantics and provenance

Per sample, choose Rise if target >= held value, otherwise Fall. With positive time T, advance toward the target by no more than `1 / (sampleRate * T)` each sample and stop exactly at the target without overshoot; T=0 assigns target directly. Parameter changes change step limits immediately; there is no generic amplitude ramp on either rate. The held value is `double` and the public output is mono `float`. Valid finite extreme CONTROL values are not secretly clipped. Nonfinite control samples are held rather than written into persistent state. This is linear slew, not an exponential lag approximation.

## Work and state contract

All mutable DSP state is private to the created instance. `create` uses nothrow allocation and `destroy` frees it; `prepare` validates and initializes its fixed state. `process`, `reset` and `event` perform no dynamic allocation, locking, I/O or host callbacks. Work is O(frames) with fixed per-sample operations, no unbounded loops or recursion. Every output writes each requested frame, including null/unpatched inputs. Empty spans do not change state. Events are applied at their caller-supplied sample boundary; sample-position values are not used to conceal block-dependent processing.

## Source and limits

No external third-party DSP implementation, presets, device driver, font or runtime dependency was included. The math and test code are original; the only implementation pattern adopted from the kit is its descriptor/callback ABI and existing GPLv3 `LinearRamp` contract. All Linux offline outcomes are reproducible at the verified toolchain, but neither operating-system realtime guarantees nor listening quality follows from offline assertions. Integration tests on Windows and the actual application, including save/load and audio audition, remain necessary; Raspberry Pi has not been tested.
