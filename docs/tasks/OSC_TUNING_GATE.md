<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC: Macro gate input, honest oscillator tuning, live frequency (from #337)

Compass rulings, 2026-11-07, on ChatGPT's handoff in #337. The work is split into three issues so each can be built and reviewed alone. **No existing ID, default or DSP result changes. Saved patches play exactly as before.**

**Read first:** `AGENTS.md`, #337 (what the code does today), `docs/SDK_AUTHOR_GUIDE.md` §2–§4 (IDs, units: pitch 1.0/oct, 0 = C4), `native/modules2/macro_oscillator/dsp.h:10-30,110-130`, `native/modules2/vco/dsp.h:11-30`, `apps/studio/public/panels/`, `apps/studio/src/panels/widgets/Knob.ts`. `wirescape_modules.json` is generated (`python tools/gen_ui_catalogue.py`).

## OSC-1 Macro `gate` input (tier T2: third-party adapter + port semantics; Keel reviews)
- Add input `gate` (hint `WS2_GATE`, poly) **at the end** of the input list. `trigger` stays as it is, so trigger-only patches render bit-identically (test it against a golden made before the change).
- Semantics, using the paths Plaits already has (no new DSP):
  - **Strike:** the value Plaits sees as `trigger` = max(`trigger`, `gate`); `trigger_patched` = either is connected. A rising gate edge strikes like a trigger.
  - **Hold:** if `gate` is connected and `level` is **not**, then `level` = the gate value and `level_patched` = true. The LPG stays open while the key is held and closes with its own decay/colour when the gate falls. If `level` is patched, `level` wins and `gate` only strikes.
- Result: Keyboard `gate` → Macro `gate` plays held notes with no ADSR.
- Tests: held gate sustains (RMS at 0.5 s > 50% of the RMS at 50 ms); low gate releases (RMS falls 40 dB within 1 s at default decay); repeated notes each strike; gate plus trigger together strike once per edge; trigger-only golden unchanged; poly voices independent.
- Panel: show `gate` next to `trigger`. Regenerate the catalogue.
```acceptance
tier: T2
size: <= 300 changed lines
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_macro_oscillator" --output-on-failure
  - python tools/dev.py test
  - cd apps/studio && npm ci && npm test
stops: [ABI header edit, changing trigger/level behaviour for patches without gate, upstream Plaits source edits]
```

## OSC-2 Honest tuning display and musical dials (tier T1: UI only, no DSP change)
- **VCO:** the DSP stays as it is. When `pitch` is connected, `frequency` is a ratio against 220 Hz. The panel must say so:
  - With a pitch cable, the knob reads **Tune** as semitones + cents from 220 Hz (220 → "0 st 0 ct", 440 → "+12 st"). Double-click resets to 220 (0 st).
  - Without a pitch cable, it reads Hz plus the nearest note and cents ("261.6 Hz · C4 +0 ct").
- **Dial range and taper (all oscillators with a Hz knob):** a log taper over C0 (16.35 Hz) to G9 (12,543.85 Hz), labelled with note names at the ends. Values outside that range stay possible (typed or saved; LFO-rate VCO patches keep working). The dial pins at its end and shows the true value; nothing is rewritten. **The DSP range is not clipped.**
- **Macro:** `coarse`/`fine` show as note + cents relative to C4.
- A shared helper in `apps/studio/src/` does Hz ↔ note/cents (A4 = 440, MIDI 69, C4 = 261.6256, MIDI 127 = G9 ≈ 12,543.85). Unit tests cover those points and octave ratios.
- Native tracking test (tests only, no DSP edit): VCO pitch 0 → 261.63 Hz at frequency 220; pitch 0.75 → 440 Hz; 1.0/oct exact; NOTE_ON 69 → 440 Hz. Macro: pitch 0 → C4.
```acceptance
tier: T1
size: <= 600 changed lines
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - cd apps/studio && npm ci && npm test
stops: [DSP change, ID or default change, patch rewrite on load]
```

## OSC-3 Live effective-frequency readout (tier T1; **blocked on #143 / Phase 3A telemetry**)
- Under the knob, a second line shows the **engine-reported** pitch per voice ("C4 +3 ct · 263 Hz", "4 voices" in poly). It comes from telemetry v1, not the knob value. With nothing playing it shows "—". With FM it's labelled "base (FM not shown)".
- FakeEngine supplies the same field, so Playwright can test it. Don't start until 3A defines the snapshot field. Add `effective_pitch` per voice to the 3A field list now (Compass note on #143).

Signed: Compass (Claude), 2026-11-07
