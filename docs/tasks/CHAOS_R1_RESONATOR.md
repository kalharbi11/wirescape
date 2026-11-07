<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS-R1 #317: `essentials.resonator`, the CHAOS body as a playable module (module lane row 4)

**What.** Step 1 of the CHAOS rebuild (`docs/planner/MODULES.md`, "CHAOS rebuild path"): CHAOS-lite's three-comb resonant body as its own polyphonic module. Strike it with the built-in exciter or feed it any audio, play its pitch from a keyboard: plucked strings, bars, metallic bodies, drones from noise. **Original code**: start from `native/modules2/chaos_lite/comb_body.h` (our own GPL code) by **copying and adapting into the new folder**. Don't edit `chaos_lite/`; its output must stay bit-identical (its tests pass unchanged). This is design A of `docs/design/ports/CHAOS_DECOMPOSITION.md` §3 "Comb Resonator", as a non-external module; there is no sst/Surge code and no `ports.` ID.

**Read first:** `docs/planner/MODULES.md` (common rules), `docs/SDK_AUTHOR_GUIDE.md` §1–§12, `native/modules2/chaos_lite/` (`comb_body.h`, `dsp.h` prepare/memory, `tests.cpp`), `native/modules2/vcf/` (poly processor with pitch).

## Interface (IDs final once merged)
| Kind | ID | Name | Details |
|---|---|---|---|
| in | `audio` | Audio | exciter audio, `WS2_POLY` (mono broadcasts to all voices) |
| in | `pitch` | Pitch | pitch, `WS2_POLY`; unconnected = 0 (C4) |
| in | `strike` | Strike | trigger, `WS2_POLY`: each rising edge fires the internal exciter on that voice |
| out | `audio` | Audio | poly, voice count from the inputs (cap 8 active voices like CHAOS-lite; voices 9–16 output silence) |
| param | `tune` | Tune | −24..+24 semitones, default 0, added to `pitch` |
| param | `structure` | Structure | stepped 0–3: Body (CHAOS ratios 1, 1.4983, 2.003), Harmonic (1, 2, 3), Bar (1, 2.756, 5.404), Bell (1, 2.32, 4.25); default 0 |
| param | `decay` | Decay | 0.05–20 s, `WS2_LOG`, default 1.5: T60 of the fundamental (per-tap gain from each tap's own period) |
| param | `damping` | Damping | 0–1, default 0.3: loss of highs per pass |
| param | `strike_tone` | Strike tone | 0–1, default 0.5: the exciter, a ~5 ms deterministic noise burst through a one-pole (0 = soft mallet, 1 = bright pick) |
| param | `strike_level` | Strike level | 0–1, default 0.8 |
| param | `mix` | Mix | 0–1, default 1 (wet only); dry is the `audio` input |
| param | `level` | Level | 0–1, default 0.7 |

## Behaviour decisions (Compass)
- The loop keeps CHAOS's character: linear-interpolated (or better) fractional delay, one-pole damping, the `x/(1+|x|)` saturator in the loop, so hot input or `decay` = 20 s rings but never grows. Output passes a DC blocker and stays finite, ≤ ±2.
- Lowest pitch at least 20 Hz at 96 kHz (size the lines in `prepare`, `WS2_LARGE`, for 8 voices × 3 taps). Pitch above the line's minimum length clamps (no NaN, no wrap).
- Exciter noise uses `ws::Random` with a fixed per-voice seed: reset gives identical strikes.
- Pitch and structure changes while ringing glide (smoothed delay time), no clicks.
- No state beyond params; lines clear on reset/load. CONSTANT never set while a voice rings.
- Category "Sound" (or nearest). A starter patch is **not** part of this task; R3 adds the CHAOS starter later.

## Tests (guide §12 list, plus behaviour with independent math)
1. Strike at pitch 0 (261.63 Hz), Harmonic structure, damping 0: the strongest spectral peak (FFT or Goertzel in the test) is within ±1% of 261.63 Hz; Body structure shows peaks near 1×, 1.4983×, 2.003×.
2. Decay: the RMS envelope after a strike falls 60 dB within `decay` ±25% at 0.5 s and at 4 s.
3. `strike` edges at known frames: onset begins on that frame; no strike → silence with `audio` unpatched.
4. External exciter: an impulse on `audio` rings at the same pitch as a strike.
5. Voices independent (two pitches → two peaks on their own channels); 16-voice input → voices 9–16 silent and finite.
6. Stability: `decay` 20 s, damping 0, ±1 noise on `audio` for 30 s → finite, ≤ ±2.
7. `chaos_lite` tests still pass unchanged (they run in `python tools/dev.py test`).
8. Frame/rate matrix, reset, state, extremes, no-allocation (guide §12).

```acceptance
tier: T1
size: <= 800 changed lines, <= 10 files in native/modules2/resonator/ (standard wiring and generated files don't count)
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_resonator_tests|abi2_chaos_lite_tests" --output-on-failure
  - python tools/dev.py test
  - cd apps/studio && npm ci && npm test
  - pi-run: python tools/module_cpu_probe.py resonator   # after #304 adds the probe; two instances × 8 voices, 48 kHz / 128 frames, worst block < 50% of the period
stops: [ABI header edit, new dependency, third-party code, edits to native/modules2/chaos_lite/, test loosening, ID change after merge]
```
Signed: Compass (Claude), 2026-11-07
