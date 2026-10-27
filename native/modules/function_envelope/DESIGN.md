# Function Envelope — DSP design

## Algorithm, semantics and provenance

On each gate edge, latch the start level, endpoint, Curve, and duration `N=max(1,llround(seconds*sampleRate))`. The sample immediately following that edge has `u=1/N`; the Nth sample reaches its endpoint exactly. For `k=4*Curve`, shape is `F(u)=expm1(k*u)/expm1(k)` and at k=0 uses `F(u)=u`. Output is `start + (endpoint-start)*F(u)`; negative Curve is fast-start, positive slow-start. Interrupted segments restart continuously from current level without extra smoothing. All state fits fixed fields; rendering has one bounded operation per sample. This is an original one-channel gated AR function, not an analog Maths circuit emulation or full Maths clone.

## Work and state contract

All mutable DSP state is private to the created instance. `create` uses nothrow allocation and `destroy` frees it; `prepare` validates and initializes its fixed state. `process`, `reset` and `event` perform no dynamic allocation, locking, I/O or host callbacks. Work is O(frames) with fixed per-sample operations, no unbounded loops or recursion. Every output writes each requested frame, including null/unpatched inputs. Empty spans do not change state. Events are applied at their caller-supplied sample boundary; sample-position values are not used to conceal block-dependent processing.

## Source and limits

No external third-party DSP implementation, presets, device driver, font or runtime dependency was included. The math and test code are original; the only implementation pattern adopted from the kit is its descriptor/callback ABI and existing GPLv3 `LinearRamp` contract. All Linux offline outcomes are reproducible at the verified toolchain, but neither operating-system realtime guarantees nor listening quality follows from offline assertions. Integration tests on Windows and the actual application, including save/load and audio audition, remain necessary; Raspberry Pi has not been tested.
