<!-- SPDX-License-Identifier: GPL-3.0-only -->
# MOD-Q (#315): `essentials.quantizer` (module lane row 2; Beta 0.8 musical core, pulled forward)

**What.** A polyphonic pitch quantizer: anything (LFO, Random Source, CHAOS, an envelope) in, notes of a scale out, plus a trigger whenever the note changes so it can fire an envelope by itself. Original code. Small.

**Read first:** `docs/planner/MODULES.md` (common rules), `docs/SDK_AUTHOR_GUIDE.md` §1–§12 (§4 pitch units: 1.0/oct, 0 = C4), `native/sdk/template/`, `native/modules2/scale_offset/` (simple poly CV processor), `native/sdk/examples/random_source/` (`ws::Schmitt`, `ws::Trigger`).

## Interface (IDs final once merged)
| Kind | ID | Name | Details |
|---|---|---|---|
| in | `pitch` | Pitch | pitch, `WS2_POLY` (sets the voice count) |
| in | `trigger` | Trigger | trigger, `WS2_POLY`; when connected, each voice samples-and-holds on its rising edge; unconnected = continuous |
| out | `pitch` | Pitch | quantized pitch, poly |
| out | `changed` | Changed | trigger (≥1 ms, `ws::Trigger`) on each voice whenever its output note changes, poly |
| param | `scale` | Scale | stepped 0–13: Chromatic, Major, Natural minor, Harmonic minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Major pentatonic, Minor pentatonic, Blues, Whole tone, Custom; default 1 |
| param | `root` | Root | stepped 0–11 (C…B), default 0 |
| param | `shift` | Shift | stepped −7..+7 **scale steps**, default 0 (applied after quantizing; moves within the scale) |
| param | `octave` | Octave | stepped −3..+3, default 0 |
| param | `n0`…`n11` | C…B | stepped 0/1, the Custom scale's notes relative to C (not to root); defaults = major (1,0,1,0,1,1,0,1,0,1,0,1). Used only when `scale` = Custom |

## Behaviour decisions (Compass)
- Nearest note of the active scale, ties go down. **Hysteresis:** the current note holds until the input moves 0.1 semitone past the midpoint to the next allowed note (no chatter on a slow noisy LFO).
- Custom scale with no notes on → passes Chromatic (never silence, never NaN).
- `shift` walks scale degrees with octave wrap (shift +1 on the top degree goes to the root an octave up).
- Output is exact: an allowed note's pitch is `(midi − 60)/12` exactly, so downstream oscillators are in tune.
- Panel: `n0`…`n11` show only when `scale` = Custom using `visibleWhen` from #306. Category "Pitch" (or the nearest existing one).
- No state beyond params; held notes reset on reset/load. CONSTANT allowed when the input is CONSTANT and no trigger is pending.

## Tests (guide §12 list, plus behaviour with independent math)
1. Sweep −2..+2 V for each built-in scale: every output equals a brute-force nearest-allowed-note table built independently in the test (interval lists, not the module's tables).
2. Hysteresis: input at a note boundary ± 0.05 semitone noise: no output change and no `changed` trigger; ± 0.2 semitone crossing: changes.
3. Trigger connected: output updates only on rising edges; `changed` fires on the frame of the change, once.
4. Root = D (2) major yields D E F# G A B C#. Shift +1 from C in C major → D; from B → C an octave up.
5. Custom all-off → chromatic. 16 voices independent; mono pitch broadcast; `trigger` mono broadcast.
6. Frame/rate matrix, reset, state, extremes, no-allocation (guide §12).

The nominal implementation budget is <= 700 changed lines and <= 8 files inside `native/modules2/quantizer/`. The current module-folder implementation is 788 changed lines, within the standing workflow +50% allowance (788 <= 1,050); the nominal 8-file limit is retained. Standard wiring and generated files are outside that module-folder budget. The executable acceptance cap covers the whole PR: 1,500 changed lines and 22 files, including native registration/build wiring, catalogues, documentation, and the visibility regression (the reviewed starting diff is 1,253 lines across 19 files).

```acceptance
tier: T1
size: <= 1500 changed lines, <= 22 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_quantizer_tests" --output-on-failure
  - python tools/dev.py test
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser="$(command -v chromium || command -v chromium-browser || command -v google-chrome)" && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
stops: [ABI header edit, new dependency, test loosening, ID change after merge]
```
Signed: Compass (Claude), 2026-11-07
