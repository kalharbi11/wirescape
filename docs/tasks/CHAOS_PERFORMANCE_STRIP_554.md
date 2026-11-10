<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #554 CHAOS performance strip (Studio T1)

Issue #554. Add a compact editable strip to the existing CHAOS module card:
Tone (shape_a, shape_b, detune), Motion (drift, lfo_depth, lfo_rate,
lfo_shape), Edge (roughness, depth), Body (body_mix, feedback), and
Brightness (cutoff). Each control reads the real descriptor/document value
and uses the normal setParam/Undo path. The original 01–06 sections stay
intact and synchronized. No macro fan-out, native DSP, catalogue/ID,
patch-format, or dependency change.

Read first: `apps/studio/AGENTS.md`, `apps/studio/docs/DESIGN.md`,
`apps/studio/src/panels/PanelBuilder.ts`,
`docs/PATCH_DESIGNER_GUIDE.md`. Regression: FakeEngine Playwright
covers compact value editing, exact numeric display, the full panel,
single Undo/Redo and reverse synchronization.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc "cd apps/studio && npm run typecheck"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [native/catalogue format change, parameter ID change, new dependency, test weakening]
```

Signed: ChatGPT (GPT-6, Studio T1 builder), 2026-11-10.
