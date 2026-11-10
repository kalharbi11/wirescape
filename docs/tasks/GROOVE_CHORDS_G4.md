<!-- SPDX-License-Identifier: GPL-3.0-only -->
# GROOVE-G4: chord steps (issue #514)

Groove note steps play up to 5 keys at once. **Pre-ruled by Keel (2026-11-10)**: the
format and identity choices below are decided, so none of them is a Keel stop. G4a is
T2 (patch format): Coda reviews and approves. G4b is T1: Codex Lead or any other
session reviews.
Read first: `docs/tasks/GROOVE_PLAYABLE_V1.md` ("Chord-ready"), `docs/tasks/CHORD_MODULE_403.md`
(rulings 1 and 5), `native/modules2/groove/`. Start after #505 (G1) merges; don't wait for #511.

Two ways to get chords, both kept: the **Chord module** after Groove (`t1_notes → Chord`)
applies one shape to every note; **G4** stores a different chord on each step.

## G4a: native (T2)
- **Format:** the reserved step key `keys`, on `note` steps only: a JSON array of 1–5
  distinct integers 0..127 with `keys[0] == key`. The UI writes both; old readers that
  only know `key` still get the root. `keys` on a rest or tie step, or one that is
  empty, too long, out of range, duplicated or disagreeing with `key`, is refused.
  The config version doesn't change: engines without G4 already refuse `keys`. No
  other new field.
- **Emission:** every key gets a NOTE_ON at the same offset with the step velocity
  and track channel, in array order. Each voice has its own `note_id`, unique among
  the instance's live and pending notes, with the instance tag as in Chord ruling 1.
  A one-key step emits **byte-identical** events to G1 (golden test).
- **Behaviour:** a tie extends every voice; gate-off and boundary release free every
  voice; a failed condition skips the whole chord and its ties; mods update once per
  step; `fired` counts the step once. Telemetry is unchanged.
- **Capacity:** pending releases grow to 5 per track. If the event sink refuses part
  of a chord, record only the voices that were emitted and release exactly those
  (as in Chord ruling 5). No allocation, locks or I/O.
- **Tests:** decode accept/refuse for each rule above; 3-key and 5-key chord at one
  offset with distinct IDs; tie across a bar; gate 0.25 releases all; condition
  skip; stop/reset releases all; partial refusal; 1/17/128/max frames; zero
  allocations; v1/G1 golden unchanged; engine test Groove → `essentials.notes_to_cv`
  (#512, or the test NotesToCv) renders 3 voices.

## G4b: Studio (T1)
- PatchDoc reads and writes `keys`, preserving unknown data; save/reload round-trip.
- Roll: shift-click (and long-press on touch) on a step that has a note adds or
  removes a key; plain click edits the root as today. The Roll already draws
  every note.
- Pads: the root as today plus a small "+N" badge. The full multi-note pad design
  is the owner's work with the UI designer and is out of scope here.
- Apply-at-loop-end and the G2 flows are unchanged. Patch guide: chord steps, limits
  (5 keys) and a Groove → Notes→CV wiring.

```acceptance
tier: T2
size: <= 900 changed lines, <= 14 files (G4a + G4b may be separate PRs, each within this)
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'groove|ui_patch_conformance|sidecar' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/studio && npm ci && npm test'
  - pi-run: ctest --test-dir build/native -R groove --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [ABI/SDK header change, Groove parameter/port ID change, v1/G1 rendering change, more than 5 keys, protocol change, strum/arp]
```

Signed: Keel (Claude), 2026-11-10
