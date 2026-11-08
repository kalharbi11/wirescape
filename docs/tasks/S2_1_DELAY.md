<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S2-1 (#314): `fx.delay`, a stereo delay (Track S2, module lane row 1)

**What.** A clean, musical stereo delay for jamming on the Pi: echoes that can sync to a patched clock, feedback that can run away without blowing up, a tone control in the loop, and ping-pong. Original code. No tape (owner, 2026-10-27): no wow/flutter, no saturation model beyond a soft limiter in the loop.

**Read first:** `docs/planner/MODULES.md` (common rules), `docs/SDK_AUTHOR_GUIDE.md` §1–§12, `native/sdk/template/`, `native/modules2/vca/` (a simple processor), `native/modules2/chaos_lite/` (stereo-pair outputs, `WS2_LARGE` delay memory), and PR #304's file list for the standard wiring.

## Interface (IDs final once merged)
| Kind | ID | Name | Details |
|---|---|---|---|
| in | `left` | In L | audio, `WS2_SUM`, stereo pair with `right` |
| in | `right` | In R | audio, `WS2_SUM`; **unconnected → reads `left`** |
| in | `clock` | Clock | trigger/gate, `WS2_FIRST`; when connected, delay time follows the clock |
| out | `left`, `right` | Out L/R | one voice each, stereo pair |
| param | `time` | Time | 0.001–2.0 s, `WS2_LOG`, default 0.375 |
| param | `division` | Division | stepped 0–7 = ×1/4, ×1/3, ×1/2, ×2/3, ×3/4, ×1, ×3/2, ×2 of the clock period; default 5 (×1). Used only while `clock` is connected |
| param | `feedback` | Feedback | 0–1, default 0.4. 1.0 = sustained repeats held at about the input level by the loop limiter, never growth |
| param | `tone` | Tone | −1..+1, default 0: in the loop, −1 dark (low-pass ~800 Hz), 0 flat, +1 thin (high-pass ~1.5 kHz) |
| param | `mode` | Mode | stepped 0 = Stereo, 1 = Ping-pong (input summed to mono and bounced L→R), default 0 |
| param | `mix` | Mix | 0–1, default 0.35, equal-power dry/wet |

## Behaviour decisions (Compass; ask in the issue before changing any)
- **Time changes glide** (smoothed read position, ~50 ms), giving a short pitch bend, never a click. Fractional reads use cubic (Hermite) or better interpolation.
- **Clock sync:** the period is measured from rising edges (`ws::Schmitt`), clamped so the delay stays ≤ 2.0 s; a new period applies after two consistent edges; if no edge arrives for 4 s, keep the last time. When the clock is disconnected, `time` rules again.
- **Loop safety:** the feedback path has the tone filter, a DC blocker and a soft limiter (e.g. `tanh`-like, ceiling about ±1.5); output is always finite and ≤ ±2 at any setting with a ±1 input.
- Memory: 2 lines × 2.0 s × prepared rate (≤ 96 kHz) floats, `WS2_LARGE`, requested in `prepare`. No state beyond params; lines clear on reset and load.
- CONSTANT: never set while any line holds non-zero signal (tail still ringing).
- A panel override is optional; default generic panel is fine. Category "Effects".

## Tests (guide §12 list, plus behaviour with independent math)
1. Impulse in, `mix`=1, `feedback`=0, `time`=0.1 s at 48 kHz: the only echo is at frame 4800 (±1 for interpolation) on both channels.
2. Feedback 0.5: successive echo peaks fall by 0.5 each (±2%) with `tone`=0.
3. Ping-pong: echoes alternate L, R, L… starting with L.
4. Clock at 120 BPM quarter notes (0.5 s) with `division`=×1/2: echo at 0.25 s after the two-edge settle.
5. `mix`=0 is the dry signal bit for bit; `mix`=1 has no dry component.
6. `right` unconnected: a signal on `left` appears on both outputs in Stereo mode.
7. Feedback 1.0, ±1 noise in for 10 s: output finite and |x| ≤ 2 throughout; time sweep 0.001→2 s while playing stays finite with no step larger than the limiter ceiling.
8. Frame/rate matrix, reset, state, extremes, no-allocation (guide §12). Poly input summed (two voices of 0.25 = one of 0.5).

```acceptance
tier: T1
size: <= 900 changed lines, <= 10 files in native/modules2/delay/ (standard wiring and generated files don't count)
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_delay_tests" --output-on-failure
  - python tools/dev.py test
  - cd apps/studio && npm ci && npm test
  - pi-run: python tools/module_cpu_probe.py delay   # after #304 adds the probe; two instances, 48 kHz / 128 frames, worst block < 50% of the period
stops: [ABI header edit, new dependency, file I/O in a module, test loosening, ID change after merge]
```
Signed: Compass (Claude), 2026-11-07
