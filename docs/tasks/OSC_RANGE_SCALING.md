<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC In: easy range scaling with Set max / Set min (#742)

**Owner fix for v1.0 (Keel, 2026-11-13; D20 / #597).** A knob sending 121…1689 clamps flat against the default 0..1 input range. The four range keys already exist; make them findable and add one-press capture. **UI-only:** no engine, native, protocol or patch-format change.

**Read first:** `apps/studio/AGENTS.md`, `apps/studio/src/app/OscEditor.ts`, `apps/studio/src/canvas/ModuleCard.ts` (OSC card, Monitor card), `MonitorEntry` in `src/engine/EngineClient.ts`, `FakeEngine.setMonitorEntries`, `tests/osc-editor.spec.ts`, `tests/osc-monitor.spec.ts`, `docs/PATCH_DESIGNER_GUIDE.md` §"OSC In and Monitor". Read-only: `native/modules2/osc_in/dsp.h:185-195` (mapping), `native/io/osc/pattern.h` + `native/io/osc/tests.cpp` (address matching).

**Mapping (engine, unchanged):** `out = out_min + clamp((raw - in_min)/(in_max - in_min), 0, 1) * (out_max - out_min)`. The engine requires `in_max > in_min` but accepts `out_min > out_max`, so a reversed knob is Custom output 1 → 0.

## 1. Range block per CV row (editor)
- Right after Address/Kind: **Raw min** / **Raw max** = the existing `input_min` / `input_max` inputs, relabelled, same test IDs.
- **Output** select `osc-row{N}-output-mode`: Unipolar 0..1 (out 0/1), Bipolar −1..1 (out −1/1), Custom (shows the existing Output min/max inputs). On open, infer: (0,1) → Unipolar, (−1,1) → Bipolar, else Custom, so unedited configs save deep-equal.
- One preview line: `121 → 0, 1689 → 1 · outside is clamped · smoothed 5 ms` (actual values).
- Argument, Smoothing, Timeout, On timeout move into a per-row `<details>` **Advanced**, closed by default; inputs and test IDs kept.
- Gate/trigger rows: no Output select, fields as today.

## 2. Set max / Set min buttons
- Per CV row: `osc-row{N}-set-max`, `osc-row{N}-set-min`, plus a live `osc-row{N}-raw` readout (`raw 1689`).
- Source: the newest entry in `telemetry.monitor[id]` of a Monitor whose `events` is cabled **directly** from this module's `messages`, whose `address` matches the row pattern (TS port of `pattern.h`: `?`, `*`, `[...]`, `[!...]`, `{a,b}`; in its own file with unit tests mirroring `native/io/osc/tests.cpp`), value `args[argument]`. App hands the editor a small read callback; the editor doesn't reach into App.
- Press writes that number into Raw max / Raw min (not saved until Save). If the result has max ≤ min, show the existing row error and keep Save disabled until fixed.
- Disabled, with a visible reason: no direct Monitor ("Patch OSC In messages → Monitor events to capture."), `argument` > 3 ("Capture reads arguments 0–3."), row off, `monitorAvailable` false, or no matching value yet ("Move the control first.").
- Never commits a graph; survives the Monitor clearing on a commit (readout shows "—" until a new value arrives).
- Replace the stale hint at `OscEditor.ts:198` ("…arrives in #604").

## 3. Clamp hint on the card
Under each enabled CV row on the OSC In card, when Monitor data for that row exists and the latest raw value is outside [Raw min, Raw max]: `raw 1689 > max 1 (clamped)`. Absent without Monitor data; never invented.

## 4. Docs
`docs/PATCH_DESIGNER_GUIDE.md` §OSC In: "Scaling a controller": the 121…1689 example with Set max/Set min and with typing; Unipolar vs Bipolar for modulation; reversed knob via Custom 1 → 0; the direct-Monitor requirement; clamping; smoothing is on for every CV row (5 ms default, Advanced).

## Hard limits (breach = stop)
No new config keys; no native/engine changes; keep every existing `data-testid` and test; don't change `defaultRow()` or preset values; unknown-key and `version` round-trip rules unchanged.

## Tests (Playwright on FakeEngine)
1. (0,1), (−1,1), (0.2,0.8) open as Unipolar/Bipolar/Custom; Save with no edits leaves config deep-equal.
2. Bipolar + Save writes `output_min:-1, output_max:1`, nothing else changes.
3. Inject `/pico1/knob/1` 1689 → Set max; inject 121 → Set min; Save commits 121/1689.
4. Set buttons disabled with the right reason: no Monitor, indirect cable, `argument` 4, row off, `monitorAvailable` false.
5. Set min above current max shows the error and blocks Save.
6. Matcher unit tests: literals, `?`, `*`, classes, negated classes, `{a,b}`, non-matches.
7. Clamp hint shows for out-of-range raw, absent without Monitor data.
8. Existing osc-editor / osc-monitor / osc-tuning specs green.

**Owner check (manual):** real controller → Set max/min on a knob, Bipolar, patch `row1` to a knob as modulation, hear the full sweep.

**Follow-ups (not here):** continuous "learn while sweeping"; raising the default smoothing if the owner test shows jitter (engine + Studio default together, T1 DSP).

```acceptance
tier: T1
budget: 1 Codex session
size: <= 600 changed lines, <= 10 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - ci: guard green          # Studio Playwright runs inside the acceptance job
stops: [native/engine change, new config key, changed/removed existing test or data-testid, new dependency]
```
