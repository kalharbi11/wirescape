<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove: external clock, run and reset inputs (#614, from #597 item 11)

This is a Compass product decision from the owner's v1.0 test: Groove needs to follow an external clock. `sequencing.groove` has no inputs today (`inputs{}`). This packet adds three **new, appended** inputs. Existing outputs, parameters, config keys and patches are unchanged. T1: module DSP plus the Studio card. No SDK, ABI or protocol change.

Read first: `native/modules2/groove/{README.md,dsp.h,tests.cpp}`, `native/modules2/clock/`, `docs/MODULE_BUILD_GUIDE.md` §B (registration and catalogue regeneration), `apps/studio/AGENTS.md`.

## Behaviour (IDs are final)
**Inputs:**

| ID | Hint | Unpatched |
|---|---|---|
| `clock` | `WS2_TRIGGER` | 0 |
| `run` | `WS2_GATE` | 0 |
| `reset` | `WS2_TRIGGER` | 0 |

All three are mono (FIRST channel) and read with `ws::Schmitt`.

- **`clock` patched:** the internal BPM stops driving steps. Each rising edge is **one 16th-note tick**, the same tick `samples_per_step` produces today, so it matches the Clock module at `ticks_per_beat` 4. Per-track `div`, `length` and the launch-queue rules work exactly as they do on the internal clock.
  - **Gate length:** derived from the measured interval between the last two pulses. Until two pulses have arrived, the pattern's `bpm` is the fallback.
  - **Pulses stop:** the sequence holds where it is. A note that is already sounding still releases at its computed gate length, so no note is ever stranded.
- **`clock` unpatched:** internal BPM, exactly as today (a regression test proves it is bit-identical).
- **`run` patched:**
  - A rising edge is START. It starts at step 1 on the next tick when stopped, so an external master isn't delayed by bar queuing.
  - A falling edge is STOP: it takes effect now and releases notes in the same frame.
  - The `run`/`stop` buttons still work.
- **`reset`:** a rising edge acts like the RESET parameter, with the same priority (RESET > STOP > MUTE > START > step advance).
- **Studio card:**
  - An **EXT** badge appears, and the BPM field is greyed with a tooltip saying it's following the clock input, while `clock` is cabled.
  - The three jacks sit on the left of the card.

## Not in this packet
- MIDI clock / DAW sync (later: a MIDI clock → Clock route).
- Swing.
- Clock-division jumpers on the input.

## Tests
- **Native:**
  - ext clock: N pulses advance N ticks, and `div` 2 halves the step rate;
  - gate length follows the pulse period;
  - pulses stopping holds the step and still releases the note;
  - `run` gate start, stop and release;
  - `reset` priority;
  - unpatched means identical output to before (compare against the existing tests);
  - frame and rate matrix, state round trip, no allocation.
- **Playwright (FakeEngine):** the EXT badge shows when `clock` is cabled.
- **Library patch:** *Clocked groove* (Clock → Groove `clock`, `t1` → VCO/ADSR) passes the offline library smoke.
- **Patch guide:** a Groove section on external clock and the wiring above.

```acceptance
tier: T1
size: <= 600 changed lines, <= 10 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [existing Groove ID/slot/config change, behaviour change with inputs unpatched, ABI/SDK header edit, protocol edit, new dependency]
```

Signed: Compass (Claude), 2026-11-11
