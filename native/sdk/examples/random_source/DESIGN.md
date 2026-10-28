<!-- SPDX-License-Identifier: MIT -->
# Random Source design notes

Provenance: original code written for Wirescape, MIT. The random generator is `ws::Random` from the SDK.

- **Per-voice state** is a fixed `std::array<Voice,16>`: no allocation after `prepare`. `reset()` reseeds every voice with `ws::Random::seed(config.seed, voice)`, so reset is deterministic and voices are decorrelated.
- **Edges** use `ws::Schmitt` per voice. `Schmitt` has no getter, so the voice mirrors its output (`high`) to detect rising edges and to save it.
- **CONSTANT fast path.** If the clock is `CONSTANT` the only possible edge is at sample 0, so we process that one sample and the held value is fixed for the block. `step` is then CONSTANT (one sample written) provided `range` is also constant, because `range` scales the output sample by sample. `smooth` is CONSTANT only if every voice has already converged to its held value (distance below 1e-6) and `range` is constant; otherwise the whole block is filled. The general loop applies the same snap rule, so the fast path equals the general path bit for bit (tested).
- **Why double for the smoothed value.** A float one-pole stops moving when `k*(target-y)` falls below half an ulp, which leaves an error near 1e-3 for second-long times. The double state converges, and the snap lets the output become CONSTANT.
- **Smooth time.** `k = 1 - exp(-1/(t*rate))` per sample, so the value reaches 1-1/e after `t` seconds at any sample rate. Constant time uses one `exp` per voice per block; a modulated time recomputes per sample.
- **Voices** is read at sample 0 of each block. Changing it mid-block takes effect on the next block.
- **State v1** is portable little-endian u32 words: per voice four words of `Random::State`, held (f32), the smoothed double as two words, and the Schmitt flag. `load_state` fills a staged copy, checks that held and smooth are finite and within range, the flag is 0 or 1 and the stream increment is odd, and only then commits. Unknown version returns `WS2_UNSUPPORTED`; short or invalid data returns `WS2_INVALID`.
- No latency, no OS facilities, no exceptions in `process` or `reset`.
