<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #488: Macro Oscillator model names in Studio

Read first: issue #488/latest handoff, `apps/studio/AGENTS.md`,
`native/modules2/macro_oscillator/DESIGN.md`,
`docs/design/ports/PLAITS_STUDY.md`, and
`apps/studio/src/panels/widgets/ModelGrid.ts`.

## Scope and source

Keep this UI-only. Map indices 0–23 to short and full engine names in one
Studio presentation table, following the exact pinned
`pichenettes/eurorack@f4cfa7697095a28d292bff758beb51deb49b5a86`
`plaits::Voice::Init` registration order. The newer eight engines precede
the classic sixteen: 7 is Chiptune, 12 Additive, 15 Speech and 20 Modal.
Do not substitute a different firmware's order.

Grid cells show the two-digit index plus a short name of at most ten
characters. Show the full name in the selected-model heading and tooltip.
Keep four readable columns at a 390 px viewport, with wrapping or shrinking
text and no overflow. Clicking a cell must still set its original numeric
`model` value. Preserve parameter/module IDs, model values and saved patches;
no native implementation or protocol change.

Audit curated labels against those actual names; synchronize the Studio source,
published patch descriptions/JSON labels and canonical starter export labels
without changing model values or wiring. Include the full index table in the
patch guide. Playwright must cover rendered names, full tooltip/heading,
numeric selection and the phone-width layout.

## Evidence

Run the complete Studio suite and typecheck. Required CI, including Web, must
be green on the exact final head. Earlier-head results are historical evidence.
The Windows sidecar seed-278 soak retains its zero-overrun threshold, original
seed and assertions; no test skip, fixture rebaseline or tolerance change is
authorized here.

```acceptance
tier: T1
size: <= 300 changed lines, <= 10 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc "cd apps/studio && npm run typecheck"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green on the exact head
stops: [model ID or value change, native/protocol change, names not traced to the pinned upstream order]
```
