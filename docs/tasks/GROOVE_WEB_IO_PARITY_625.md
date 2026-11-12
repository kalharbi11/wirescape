<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove webapp I/O parity (#625; owner priority)

Groove has 18 existing native CV/gate/event/observation **outputs** on `main`,
but the Studio stylesheet hides the entire Groove port rail at phone widths.
The browser runs the same Studio renderer over the WebAssembly bridge. Make
Groove useful on the deployed webapp without waiting for #614's clock inputs.

## Scope / owner UI decision (2026-11-11)

- **Keep the full horizontal 1250px Windows editor on phones.** It is acceptable
  for the end of the card to be offscreen; the user can pan or zoom. Do not
  compress the editor, change pattern controls, or hide the port rail.
- On web, mobile and Electron, the output jacks must be real active ports,
  including `t1_pitch`, `t1_gate`, `t1_notes` and `t2_pitch`.
- Prioritise those four jacks in the existing collapsed rail; expose the
  remaining existing ports through the existing +/− expander.
- No native DSP or SDK/ABI/protocol edits. No new module IDs or saved patch
  changes. No changes to non-Groove card layout.
- The **input** jacks `clock`, `run`, `reset` are already implemented in
  #614 / PR #619. Do not duplicate that PR or block this output-only fix.
- Neither web nor Electron may be described as production-audio validated
  merely because simulated or CI browser tests pass.

## Acceptance

- Verify the *actual* WebAssembly hello/catalogue includes Groove's current
  output IDs; do not rely only on static `public/wirescape_modules.json`.
- A real browser AudioWorklet/WASM test loads Groove pitch/gate into
  VCO/ADSR/VCA/Output, requests Groove START, observes real run/playhead and
  an audible output peak without keyboard/MIDI/OSC.
- A phone-width browser test observes a 1250px native Groove card and visible,
  interactive port rails after panning/zooming, including a visible + expander.
- The existing FakeEngine Groove G2b desktop test checks the owner-approved full-width phone layout and real port rail; all other Groove desktop tests still pass. Electron shares the
  renderer; deployment smoke and physical/touch device tests remain separate.
- Patch designer guide impact: none, CSS / card presentation only.

```acceptance
tier: T1
size: <= 250 changed lines, <= 5 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
  - ci: web-demo engine and site jobs green
stops: [Groove output ID/ABI change, change to native DSP or protocol, disable browser audio tests, hide port jacks on phones, change any other module layout]
```

Signed: Codex Lead (GPT-6), 2026-11-11
