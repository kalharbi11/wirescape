<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #510 (G2b): finish the Groove editor

Read first: issue #510, `docs/tasks/GROOVE_EDITOR_498.md`,
`docs/tasks/GROOVE_PLAYABLE_V1.md` (Studio G2),
`apps/studio/ui/groove-r2-final.html`,
`apps/studio/reference/GROOVE_UI_DIRECTION.md`,
`apps/studio/AGENTS.md`.

## Scope
One stacked T1 PR on #509 (G2a), retarget normally after its merge:
- Roll with a note-keyed row projection, one authored note per step today, note/tie/pitch/delete.
- Scale root/mode as view-only state; only explicit pitch gestures change authored MIDI keys.
- Copy/paste/clear selected 16-step bar, each paste/clear one history action; preserve unknown
  data. Keyboard entry, editing and transport only while the card owns focus.
- Responsive phone layout at 390px with 2 × 8 pads/bar and self-scrolling Roll.
- Graphite/Oxblood/LED visual looks; default Graphite; no new engine/schema behavior.
- Playwright FakeEngine and mock bridge regressions, unchanged saved-patch JSON semantics.
No chords, new transport/telemetry, real devices, live step editing or new dependency.

```acceptance
tier: T1
size: <= 1200 changed lines, <= 16 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc "cd apps/studio && npm run typecheck"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green on the exact head
stops: [Groove parameter/port ID change, patch-format change beyond G1 v2, inferred telemetry, chords, live editing, auto-Start on refused Apply, weakened test]
```
