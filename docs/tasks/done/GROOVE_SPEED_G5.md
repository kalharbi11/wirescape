<!-- SPDX-License-Identifier: GPL-3.0-only -->
# GROOVE-G5: per-track speed (issue #523)

Owner request 2026-11-10; UI from #521 (`apps/studio/reference/GROOVE_UI_DIRECTION.md`,
"Per-track speed"). **Pre-ruled by Keel + Compass (2026-11-10):** it moves into the Groove
lane after G4 (it was "out of v1"). G5a is T2 (patch format): Coda reviews and approves.
G5b is T1. No Keel stop is left.
Read first: `docs/tasks/GROOVE_PLAYABLE_V1.md`, `native/modules2/groove/`. Start after #505
(G1). G5a edits the same `config.h`/`dsp.h` as G4a, so do G4a first; one builder may bundle
G4a + G5a in one PR.

## G5a: native (T2)
- **Format:** an optional track key `div` ∈ {1, 2, 4, 8} (1×, ½, ¼, ⅛); 1 is the default
  and the UI omits it. Any other value is refused. Engines without G5 already refuse
  the key, and the config version doesn't change. No 2× and no triplets.
- **Clock:** `n` counts sixteenths from Start. A track steps on `n % div == 0`, and
  `step_t = (n / div) % length`. Onsets happen only on a track's own step boundary. The
  absolute counter wraps at 512 (every `length × div` divides 512).
- **Gate and ties:** the gate length is `gate × div` sixteenths after onset; a tie
  extends by one slowed step. Mods hold for the slowed step.
- **Loops:** a track's cycle is `length × div`. The master cycle is the maximum over
  armed tracks (≤ 512; the existing fallback applies when none are armed). Global
  STOP and Apply-at-loop-end fire at `n % master == 0`; track STOP at that track's
  own cycle end. A:B, first and not_first count the track's own cycles. Start, Reset
  and track entry reset `n` as G1 does.
- **Telemetry:** unchanged (`t*_pos` = `step_t`). The editor reads `div` from the
  config to animate the playhead.
- **Compatibility:** with every `div` = 1 the output is **byte-identical** to G1/G4
  (golden). No allocation, locks or I/O; bounded work per block.
- **Tests:** decode accept/refuse; `div` 2/4/8 onset spacing at 120 bpm; gate and tie
  scaling; master cycle with 16×¼ + 16×1 (STOP at 64 sixteenths); A:B on a slowed
  track; chords (G4) on a slowed track; 1/17/128/max frames; zero allocations; golden unchanged.

## G5b: Studio (T1)
- PatchDoc reads/writes `div`, omitting it at 1 and preserving unknown data.
- `Speed 1× ½ ¼ ⅛` next to `Steps`; the track tab shows `½×`/`¼×`/`⅛×` when slowed.
  The playhead and the loop-end preview use `length × div`.
- Patch guide: speed values, the master-cycle rule and a "4-bar progression in 16 pads" example.

G5a and G5b may be separate PRs, each within the limits below.

```acceptance
tier: T2
size: <= 700 changed lines, <= 12 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'groove|ui_patch_conformance|sidecar' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/studio && npm ci && npm test'
  - pi-run: ctest --test-dir build/native -R groove --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [ABI/SDK header change, Groove parameter/port ID change, rendering change at div 1, speeds other than 1/2/4/8, protocol change, swing/ratchets]
```

Signed: Keel (Claude), 2026-11-10
