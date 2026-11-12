<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #498 (G2a): Groove editor in Studio, Pads view

Read first: issue #498 (latest Coda comment defines the G2a/G2b split),
`docs/tasks/GROOVE_PLAYABLE_V1.md` (Keel's v2 schema and Studio G2),
`apps/studio/ui/groove-r2-final.html`, `apps/studio/reference/GROOVE_UI_DIRECTION.md`,
`native/modules2/groove/README.md` ("Stopped configuration workflow"), `apps/studio/AGENTS.md`.

## Scope

Production Studio card for `sequencing.groove` on config v2, stacked on G1 (#496) and G3 (#501):
- Load v1 and v2 patterns; the first edit writes v2 (v1 tracks keep their 32-step meaning
  as an explicit `length: 32`). Unknown keys stay in the document; the engine copy carries
  only known v1/v2 fields, and every whole-graph load (undo, restart, other edits) sends the
  *applied* pattern, never an unapplied draft.
- Pads view: two lanes (Trig, Note); select, second-click toggle, vertical drag pitch
  (Shift = octave), wheel, drag-right tie; inspector for kind, note, velocity, gate ¼–1 and
  the condition picker (chance, A:B, first, not first); step counts 8/16/32/64.
- Draft / Apply / Revert: Apply only when `engine.canApplyGroove(id)` is true (current-gen
  measured `[-1,-1]` or audio not running), through the existing whole-graph load; refusal
  keeps the draft and never starts. "Stop & apply" queues STOP and applies on the measured stop.
- Playhead, position and the "skipped this pass" dim from telemetry `playhead`/`groove` only.
- Undo/redo through the app history; save/reload equality; patch guide update.

Out of scope (G2b): Roll view, scale helper, copy/paste/clear bar, keyboard map, phone
layout, trig-look selector. Leave seams, don't build them.

```acceptance
tier: T1
size: <= 1600 changed lines, <= 20 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc "cd apps/studio && npm run typecheck"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green on the exact head
stops: [Groove parameter/port ID change, patch-format change beyond Keel's v2 schema, live editing during playback, inferred playhead or STOP, auto-Start after a refused Apply]
```
