<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1: UI designer handoff

Prepared by Codex Lead, 2026-11-08 (America/New_York).
Status: proposed design exercise; no prototype or production UI implemented here.

## Recommendation and sequencing

Do one short visual/interaction study now, alongside S1 native work. Choose the
editor structure before building the production Studio editor. Polish the result
after the first playable S1 integration, using actual owner feedback.

S0 #430 / PR #433 merged as `50380fd`; it proves one eight-step configuration,
not the finished instrument. S1 is #432. Keel's latest comment there preauthorizes
the additive v1 format, ports and transport, and suggests native (a), editor (b).
Re-read current issue comments before implementation; this brief is not a new
schema, transport or telemetry contract.

Native work can advance two synchronized tracks, two mods, queued transport,
release safety and deterministic tests while design proceeds. Integration must
establish truthful completed-STOP and loaded/refused state from existing native
telemetry/ACKs. If unavailable, bring that gap to Keel; a mock cannot solve it.

## Paste-ready designer assignment

You are the Groove UI designer for `kalharbi11/wirescape`. Build a quick design
study for a musical instrument inside Wirescape, using the existing UI kit's
visual language. The goal is readable sequencing and clear editing/performance
states. Audio and engine connectivity are not required. Simple local interactions
are useful, but static annotated states are sufficient.

Work in the current main repository, under `apps/studio/`, not the archived
`kalharbi11/Wirescape-UI` repository. Recheck current main and the latest S1
handoff; preserve other builders' ownership. For a submitted PR, create/claim a
separate design-only child issue of #432 and use `task/<issue>-groove-ui-study`.
Do not claim the native instrument or production editor just to make the study.

Read in this order:

1. Root `AGENTS.md` and `apps/studio/AGENTS.md`.
2. #432 and its latest Keel/lead handoffs, then `docs/sequencer/GROOVE.md`.
3. `apps/studio/docs/DESIGN.md` and `apps/studio/ui/wirescape-ui-kit.html`.
4. `apps/studio/docs/PERFORMANCE.md`, `DECISIONS.md`, `ARCHIVE.md` and `REFERENCES.md`.
5. `apps/studio/src/styles/sequence.css` and the current sequence panel, for shared
   control/gesture conventions. Preserve `example.step_sequencer` and its files.

### Instrument scope

- Two synchronized tracks; 32 sixteenth-note steps per track; one pattern each.
- Shared BPM, fixed 1x playback, looping; one selected track editor, pages 1–16
  and 17–32. Keep both tracks' performance status visible while editing one.
- Step Rest / Note / Tie, pitch, velocity, Mod1 and Mod2. Use note names for
  musical readability; exact-value entry must remain discoverable. Saved `key`
  semantics and ranges come from the reviewed implementation, not the mock.
- Global and per-track Start/Stop/Mute, Reset, and armed/queued/running status.
  Global Stop waits for the shared 32-step boundary. Mute releases notes while
  phase continues; it is not permission to edit.
- Pattern edits unlock only after actual global STOP completion. A queued stop,
  one stopped track, or global mute does not unlock editing.
- Explicit draft, applying, loaded and refused states. Keep the draft on refusal;
  never imply a refused candidate is audible or ready to start. Show saved-draft
  status separately from engine-loaded status. Include basic Undo/Redo.
- Apply can reset/interrupt other modules. Communicate that near Apply in plain
  language, without a recurring confirmation modal.

For this study, use separate Apply and Start actions; after loaded confirmation,
Start begins at step 1. The roadmap says no auto-start; Keel's later wording says
Apply "then restarts from step 1." Flag this wording for builder/Keel reconciliation
before production behavior is wired. Do not silently choose automatic playback.

Gate duration choices and selected-step/page copy/clear/duplicate are planned
S1b extras if deferred. Show them only in a separately labelled optional study.
Do not add swing, ratchets, probability, memories, scenes, song mode, live editing,
64-step editing or visible unused tracks 3/4.

### Design exercise

Explore two lightweight alternatives, then develop one recommendation:

1. A 16-step grid with lane switching for pitch/velocity/mods and a persistent
   selected-step inspector for exact values.
2. A 16-step grid with stacked value lanes, comparing its scanability and size
   against the first alternative.

Keep the single selected-track editor in both. Compare track/page switching,
number of gestures, legibility, touch targets and patch-canvas footprint. Include
a compact in-patch card and an expanded editor study; propose the expansion
placement rather than assuming a new app navigation system.

Use existing tokens, type, numbers, segmented controls, latches, borders and port
rails. Rust marks live values/events; selected track, selected step and unsaved
draft use neutral structure plus text. Distinguish selected step from playing step.
Make beat groups of four readable and page boundaries unambiguous. Browsing page
2 must not move playback or erase page 1. Do not auto-switch pages while someone
is editing; make off-page playback legible.

Use at least 44 px touch targets for editable controls, visible keyboard focus,
non-colour status labels and direct-value entry. Provide mouse and touch density
examples. Check 1280×720 and 1920×1080, and a narrower viewport with a documented
fallback. These are study sizes, not verified physical Pi display specifications.
Do not shrink controls below usable size to fit all lanes.

### Required state examples

Show stopped/loaded, stopped/dirty, running/locked, stop queued/still locked,
applying, refused/draft retained, disconnected or restarted/confirmation unknown,
track muted/running, track start queued and an off-page playhead. An unavailable
native status must read unknown/disconnected rather than stopped or loaded.

Use labelled fixtures or a clearly separated "Demo state" control for simulated
positions/ACKs. No timer-driven playhead presented as engine truth. Static examples
are fine. Any motion follows the existing single-loop, visibility and reduced-motion
rules; do not add animation libraries or new runtime dependencies.

### Files to deliver

- `apps/studio/ui/groove-s1-study.html`: openable study, two rough alternatives,
  one developed recommendation, required states and an example two-track phrase.
  Reuse `ui/tokens.css`; use system fallbacks so it remains readable offline.
- `apps/studio/ui/groove-s1-study.css`: scoped styles that a builder can reuse.
  Small demo-only JavaScript may live in the HTML or a sibling file.
- `apps/studio/reference/GROOVE_S1_DESIGN_NOTES.md`: chosen layout, dimensions,
  gestures, state/action table, accessibility, optional extras, unresolved decisions,
  and a map from study regions to existing Studio widgets/integration points.

Keep design/demo code separate from production `src/`, native code, generated
catalogues, protocol and patch schema. Do not rewrite the approved UI kit or
design-system decisions. Put SPDX headers on new source files. Keep screenshots
as review artifacts rather than committed binary assets.

### Review and completion

Walk through: create a note/rest/tie phrase on Track 1; edit steps 17–32; switch
to Track 2; distinguish selected versus playing step; request Stop and observe
the locked queue; edit only after completed Stop; Apply successfully; Start from
step 1; demonstrate refusal retaining the draft; mute without losing phase.

For static work, annotate each action and resulting state. For implemented local
interactions, verify those paths in a browser at the stated sizes/densities and
check focus, overflow and console errors. Record actual checks in the notes.
Production Playwright/FakeEngine and native/platform acceptance belong to later
integration; a design study does not certify timing, audio or Pi performance.

Done means an owner can compare alternatives, judge the main workflow and hand
the selected HTML/CSS and behavior notes to the S1 editor builder. End with files,
preview instructions, verified sizes, remaining questions and the normal signed
handoff: `Signed: <agent> (Groove UI designer), <date>`.
