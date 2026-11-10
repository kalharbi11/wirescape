<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove playable v1 (#432): the owner's UI, 64 steps, trig conditions

Umbrella #432. Three packets, each one issue/branch/PR:
**G1 native v2 config** #496 (T2) → **G2 Studio editor** #498 (T1, after G1's schema merges) and
**G3 playhead telemetry** #497 (T2, parallel with G1). Design source of truth:
`apps/studio/ui/groove-r2-final.html` and `apps/studio/reference/GROOVE_UI_DIRECTION.md`
(#495, owner-approved). Plan context: `docs/sequencer/GROOVE.md`.

## Compass: scope (2026-11-10)

The owner wants the Groove sequencer **playable** with the new UI, and every function in
that UI working. That pulls these items forward from later phases:
- **Per-track length 8 / 16 / 32 / 64 steps** (default 16 for new patterns). Each 16 steps
  is one bar row. Lengths are limited to these four for now; arbitrary 1–64 stays in S4.
- **Gate length** per step: ¼ ½ ¾ 1.
- **Trig conditions**, one per note trig: **chance %**, **A:B** ("play on loop A of every
  B"; this covers the owner's "every 2, 3, 4 bars"), **first** and **not first**.
- **Copy / paste / clear bar**, a **scale helper** (UI only), **undo/redo**, the **draft →
  Apply at loop end** flow, **Pads and Roll** views, **keyboard entry**, the **phone layout**
  and the **three trig looks** (default Graphite; a look selector stays in the UI).

Still out of scope: live per-step editing during playback (S2; edits are a draft until
Apply), chords (the roll is reserved for them), more than 2 tracks, per-track speed,
swing/ratchets, pattern memories and songs. **Chords come right after the Chord module;**
G1 and G2 must leave room for them as described under "Chord-ready" below.

Acceptance from the owner's side: on Windows (and on the Pi once it's installed), build a
16-step bass and a 64-step lead with ties, gate lengths, a 50% trig and a 1:4 trig. Play
them, change steps while playing, Apply at loop end, hear the change, save, reload, and
everything is identical.

## Keel: technical contract (2026-11-10)

**Config (patch JSON, `modules[].config` of `sequencing.groove`):** bump to `"version":2`.
v1 still loads losslessly: missing `length` means 32 (the v1 meaning) and missing step
fields take the defaults below. The UI writes v2 from its first edit. An old engine refuses
v2 (version check), so it never plays it wrong.
- Track: `length` ∈ {8,16,32,64} (default 16 for new tracks, 32 for v1), `steps` 1..length
  (padded with rests), plus the existing `armed` and `mute`.
- Step: existing `kind`/`key`/`velocity`/`mod1`/`mod2`, plus `gate` ∈ {0.25,0.5,0.75,1}
  (default 1) and `cond`, one of `{"type":"chance","p":1..99}`, `{"type":"ab","a":1..b,"b":2..8}`,
  `{"type":"first"}` or `{"type":"not_first"}`. Absent means always. `cond` is allowed only
  on `note` steps.
- Unknown keys: the engine refuses them as today. The UI preserves them (they're never
  sent, because the UI only writes known v2 fields to the engine copy).
- **Chord-ready:** reserve the step key `keys` (an array of MIDI keys) for chords. v2 refuses
  it, so a future v3 can add it additively. Natively, emit notes through one function that
  loops over the step's notes (size 1 today), so chords widen that loop, not the scheduler.

**Transient POD (`native/modules2/groove/config.h`):** keep 4×64 reserved and bump
`Config.version` to 2. Add `Track.length` (uint32) and these to `Step`: `gate_q` (uint8,
1..4 quarters), `cond_type` (uint8: 0 always, 1 chance, 2 ab, 3 first, 4 not_first),
`cond_a` and `cond_b` (uint8; `cond_a` holds `p` for chance). Update the static_assert
sizes in the same PR (this ruling approves the layout change). `track_count` ≤ 2 still.

**Timing:**
- Each track loops over its own length. Because the lengths are 8/16/32/64, every track
  realigns at the **longest active length (≤ 64 steps = 4 bars)**. That's the master cycle;
  there's no LCM.
- Global STOP and **Apply at loop end** queue to the master-cycle end. Track STOP queues to
  the track's own loop end. Track START enters at the next 16-step bar boundary, at that
  track's matching phase. RESET is unchanged.
- **Loop counter per track:** 1 on Start, +1 at each wrap. `ab` fires when
  `(loop-1) % b == a-1`, `first` when loop == 1, and `not_first` otherwise. One loop of a
  16-step track is one bar; the UI labels A:B as "loops", with "bars" as a hint when the
  track is 16 steps.
- **Chance:** a per-track xorshift32 seeded from (module id, track) at Start/Reset. It
  advances once per evaluated chance trig, so a given start is reproducible in tests.
- A false condition skips the note **and its ties** (they act as rests for that pass).
  **Mods still update on skipped steps** (conditions gate notes only), so CV stays predictable.
- Gate length: gate-off at `gate × step` samples after onset. A tie extends the note through
  the tied steps, and the last tied step's gate length sets the release.

**Realtime:** all fixed-size, with conditions evaluated at step onset. No allocation, locks or
logging in process. Module state save/restore adds the loop counters and RNG (bump the state
version; older state loads with counters = 1).

**G3 telemetry (T2, ENGINE_PROTOCOL §7 additive):** per Groove instance and track: `step`
(0-based), `loop`, `playing`, and `fired` (whether the current step's trig passed its
condition), all from the render-owned snapshot at the existing ≤30 Hz. These are the only
truthful sources for the playhead, the rust bar number and the "skipped this pass" dim.
No inferred playhead.

**Tests:**
- v1 fixture renders byte-identically before and after G1.
- Length matrix: 8/16/32/64 alignment and master-cycle STOP/Apply.
- Condition matrix: chance with a fixed seed (a count over 1000 loops lands in a tolerance
  band, deterministic per seed), ab for every b=2..8, first/not_first across Stop/Start.
- Skipped ties, gate lengths with ties, and no allocations under the existing watch.
- `pi5/accept` with the groove CTests, plus sanitizers. Keep the existing saturation tests
  from #461.

**Studio (G2):** implement r2-final against the real engine, not the demo clock:
- Pads view with lanes, gestures, inspector and cues.
- Roll view, single-note only, with the roll data model keyed per note so chords drop in.
- Steps 8/16/32/64; the draft/Apply/Revert bar uses the existing stopped Apply path and
  global STOP ACK from the README contract.
- The playhead comes from G3 telemetry, hidden until G3 merges.
- Keyboard map, undo/redo, scale helper, copy/paste/clear bar, the phone layout (<760 px),
  and the trig-look selector (default Graphite).
- Playwright with FakeEngine and the mock bridge covers: select/second-click toggle,
  drag pitch (Shift = octave), wheel, drag-right tie, the condition picker, every step count,
  Apply at loop end, save/reload equality and the 390 px layout.
- Update the patch guide in the same PR.

Signed: Compass (Claude), 2026-11-10 · Keel (Claude), 2026-11-10

```acceptance
tier: T2
size: <= 1600 changed lines, <= 20 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R groove --output-on-failure --no-tests=error"
  - bash -lc "cd apps/studio && npm ci && npm test"
  - pi-run: ctest --test-dir build/native -R groove --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on the exact head
stops: [existing Groove parameter/port ID change, v1 rendering change, live editing during playback, chords, more than 2 tracks, protocol change outside §7 telemetry]
```
