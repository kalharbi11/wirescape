<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S2-2 (#316): `fx.reverb`, a stereo reverb (Track S2, module lane row 3)

**What.** A smooth stereo room-to-hall reverb with a live **freeze**, cheap enough that two run on the Pi beside a full patch. **Original code**: an 8-line feedback delay network (Jot/Stautner–Puckette style, Householder or Hadamard mixing), input diffusion by allpasses, per-line damping. Cite the textbook sources in `DESIGN.md`. The Clouds-reverb port in `docs/reviews/EFFECTS_OPTIONS.md` stays a later option only if the owner dislikes this one; don't copy third-party code here.

**Read first:** `docs/planner/MODULES.md` (common rules), `docs/SDK_AUTHOR_GUIDE.md` §1–§12, `docs/tasks/S2_1_DELAY.md` (same port pattern; reuse its stereo-in normalling if it has merged), `native/modules2/chaos_lite/` (`WS2_LARGE` delay memory).

## Interface (IDs final once merged)
| Kind | ID | Name | Details |
|---|---|---|---|
| in | `left`, `right` | In L/R | audio, `WS2_SUM`, stereo pair; **unconnected `right` → reads `left`** |
| out | `left`, `right` | Out L/R | one voice each, stereo pair |
| param | `size` | Size | 0–1, default 0.5: scales all line lengths 0.25×–1.0× of the base set (base longest line ≈ 100 ms at 48 kHz) |
| param | `decay` | Decay | 0.2–20 s RT60, `WS2_LOG`, default 2.5 |
| param | `damping` | Damping | 0–1, default 0.4: high-frequency loss per pass (0 = bright, 1 = dark) |
| param | `predelay` | Pre-delay | 0–0.25 s, default 0.02 |
| param | `width` | Width | 0–1, default 1: 0 = mono wet, 1 = fully decorrelated L/R |
| param | `motion` | Motion | 0–1, default 0.2: slow deterministic modulation of line lengths to stop metallic ringing |
| param | `freeze` | Freeze | stepped 0/1, default 0: input muted, feedback = 1, damping bypassed, the tail holds |
| param | `mix` | Mix | 0–1, default 0.3, equal-power dry/wet |

## Behaviour decisions (Compass)
- Line lengths are mutually prime sample counts recomputed from `size` and the prepared rate; size changes **glide** (smoothed, interpolated reads), never click.
- Per-line feedback gain derives from `decay` and each line's length (standard RT60 formula), so decay time doesn't change with size.
- Freeze in/out crossfades over ~20 ms. Frozen output is bounded (energy held, not growing).
- Motion uses fixed-seed `ws::Random` or fixed-rate sines, deterministic after reset.
- Memory: all lines + predelay at 96 kHz and size 1.0, `WS2_LARGE`, in `prepare`. No state beyond params; lines clear on reset and load. CONSTANT never set while the tail is non-zero.

**Acceptance budget clarification:** The module's own folder retains its original <= 10-file budget. The generic acceptance runner counts standard wiring and generated catalogue files globally. Review of CI required updating the existing Studio catalogue-count assertion (16th changed file). This is a **one-file exception beyond the 15-file Workflow v3 allowance**, submitted explicitly for the independent T1 reviewer/planner to approve; Codex Lead authorized a narrow 920-line ceiling on 2026-11-08 for the glide-precision fix and non-default-size freeze regression. The module-folder cap and all behavioral tolerances remain unchanged. The accepted test assertions are unchanged; the Studio command uses the runner's supported Bash invocation and mirrors the existing Web CI Playwright ffmpeg/Chromium bootstrap.

## Tests (guide §12 list, plus behaviour with independent math)
1. Impulse at `decay`=2 s, `damping`=0, `motion`=0, `mix`=1: the energy envelope (RMS in 50 ms windows) falls 60 dB in 2 s ±20%.
2. `decay` change does not depend on `size`: RT60 at size 0.2 and 1.0 within ±20% of each other.
3. Freeze: after 1 s of noise, freeze on and input continues: output RMS over the next 2 s stays within ±1.5 dB of the RMS at freeze time, and new input doesn't enter.
4. Width: mono noise in; L/R correlation of the wet signal < 0.5 at width 1 and > 0.95 at width 0.
5. `mix`=0 is dry bit for bit. `right` unconnected mirrors `left`.
6. Stability: max decay, damping 0, ±1 noise for 30 s: finite, |x| ≤ 2. Size sweep 0→1→0 while playing: finite, no single-sample step above 0.5 with a 0.25-amplitude input.
7. Frame/rate matrix, reset, state, extremes, no-allocation (guide §12).

```acceptance
tier: T1
size: <= 920 changed lines, <= 16 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_reverb_tests" --output-on-failure
  - python tools/dev.py test
  - bash -c 'cd apps/studio && npm ci && npx playwright install ffmpeg && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - pi-run: python tools/module_cpu_probe.py fx.reverb   # after #367 adds the shared probe; two instances, 48 kHz / 128 frames, worst block < 50% of the period
stops: [ABI header edit, new dependency, third-party code, file I/O in a module, test loosening, ID change after merge]
```
Signed: Compass (Claude), 2026-11-07
