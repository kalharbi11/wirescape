<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1 design study — implementation handoff

Design-only child: #457 of #432. Governing brief: `docs/sequencer/UI_DESIGN_BRIEF.md`.
The artifact is an **offline fixture**: it does not contact the engine, read audio,
send MIDI, create sockets or represent native STOP, playhead or load ACK. Native
Groove S1(a) remains PR #451; S1(b) is a separate production integration.

## What to open

Open `apps/studio/ui/groove-s1-study.html` directly in a browser. Its sibling
`tokens.css` and `groove-s1-study.css` are relative local stylesheets. No
font download, package install, bundler or JavaScript library is needed. The
first section compares two concepts; the compact card is section 02; the
developed interactive design is section 03; section 04 is the annotated state
gallery. Every interactive transport/playhead/ACK control is labelled as demo
state. No timer advances a playhead.

## Layout decision

| Dimension | A: step grid + inspector (recommended) | B: stacked value lanes |
|---|---|---|
| Step navigation | One 16-step row with four-step beat groups and two explicit pages | The same 16-step positions repeated across pitch, velocity, Mod1 and Mod2 |
| Editing | Click a step, choose Rest/Note/Tie, type exact values in persistent inspector | Compare many values directly; needs multiple lane gestures or tiny controls |
| Information density | State/pitch are always readable; other values available at selection | All values visible simultaneously; tall and busy at 1280×720 |
| Touch | 48px minimum step width, 80px height and 44px buttons | Would need horizontally scrolling rows and additional height to keep 44px targets |
| Canvas footprint | Small card on patch; large inspector can expand beside/over it | Better as optional analytic desktop view, not primary card/editor |
| Decision | **Use A for production S1(b)** | Keep as a study only, not a shipped switch |

Use a non-modal expanded inspector anchored to the selected Groove module. Keep
the patch canvas visible when space allows; on narrow viewports the inspector
stacks and its **step row scrolls internally**, not the entire app canvas. Do
not silently add an app-wide navigation tab.

**Geometry checked in cloud Chromium (not physical Pi evidence):** outer study max
1440px; expanded editor spans the available study width; 16-cell row requires
824px minimum and scrolls at narrower widths; two equal-status track rows
stack below 600px; two inner panels stack below 900px. Intended review sizes:
1920×1080, 1280×720, and a narrow portrait viewport (e.g. 390×844).
Real Chromium screenshots and root overflow/focus checks cover all three sizes;
the narrow step row scrolls inside its container. Physical display QA remains separate.

## Fixture and gesture walk-through

The fixture starts with both tracks armed, stopped, and a *simulated* loaded
snapshot. Track 1 contains notes C4/G4/D4, a tie, and page-two notes E4/C5.
Track 2 contains G4, a tie, C5 and G3 at step 24. Values are authored
`key` (MIDI integer 0–127), `velocity`, `mod1`, `mod2` (0–1); note names
are a derived display. MIDI 60 = C4 = zero pitch CV. Rest/tie retain last
modulation as ruled for S1; velocity zero is musically a rest.

1. Click step 1, change its kind/values, then Undo/Redo. The draft changes,
   not the simulated loaded config. **Start is blocked** while they differ.
2. Switch pages 01–16 and 17–32 and select Track 2. Neither action moves
   the transport. The currently selected track is neutral-ink outlined.
3. Click Apply. The demo enters `applying`: editing and transport controls
   are locked, pending **manual** “Confirm Apply ACK” or “Refuse Apply.”
4. Click Refuse Apply. The draft remains present; the loaded graph is still
   the previous config; Start remains disabled. Click Apply again and
   manually Confirm ACK to acknowledge this draft as loaded and *stopped*.
   The study deliberately **does not automatically Start on ACK**.
5. Click Start. The demo explicitly shows step 1, no moving animation.
   Click “Show step 23” while page 1 is selected: “OFF PAGE” is shown,
   but the selected page remains page 1.
6. Click Stop. It becomes **queued and locked**, not completed; a second
   Stop cancels the queue. Stop again, then click “Complete queued STOP”
   to simulate authoritative native completion and unlock editing.
7. During running, use Global Mute/Track Mute. Neither unlocks editing.
   Track Stop queues; a subsequent Track Start cancels its pending STOP.
   Disarm a track while globally stopped, then after global Start use
   Track Start to queue boundary entry; Track Stop cancels queued entry.
8. Click Disconnect: STOP and loaded confirmation read **unknown**;
   editing and Start stay disabled. Only an explicit simulated confirmation
   re-establishes STOP and the prior acknowledged load, preserving the draft
   and any refusal. Recovery never acknowledges an unapplied candidate.

There is intentionally no keyboard shortcut that bypasses a lock, no inferred
completed STOP from a gate's level, and no browser-created transport beat.
The display labels a simulated position only when an explicit demo action
sets it. The fixture's save button records a local “saved draft” marker
without writing a file, so it cannot be mistaken for patch persistence.

## State / action contract for the production builder

| Native-observed state | Edit pattern? | Apply? | Start? | Display |
|---|---|---|---|---|
| Confirmed globally STOPPED, loaded = draft | Yes | Only if draft changes | Yes, explicit | Stopped / loaded |
| Confirmed STOPPED, draft differs | Yes | Yes | **No** | Draft dirty, not audible |
| RUNNING | No | No | No | Running, step from native |
| STOP requested/queued | **No** | No | No | Queued; waiting for confirmed completion |
| Applying candidate | No | Await ACK/refusal | **No** | Applying, generation pending |
| Apply refused | Yes, correct draft | Retry | **No** | Refused; original graph unchanged |
| Disconnected/restart, not confirmed | **No** | No | **No** | Unknown, never “stopped” |
| Global or track muted while running | No | No | No | Muted / clock still running |
| Track Start queued while global running | No | No | No | Queued entry at shared boundary |
| Playhead 23, editor page 1 | No if running | No | No | Step 23 off-page, selected page stays |

The demo does not establish how the production app obtains native truth. The
existing telemetry seam has **no installed authoritative Groove STOP/playhead**.
Keel ruled on the separate measured t1_pos/t2_pos seam in #432 comment
6735445406; that dedicated T2 integration follows #451 and is not built here. A `stop`
parameter/command receipt, muted output, or fake-browser clock is insufficient.
S1 config uses reviewed JSON `version:1`, 1–2 tracks/1–32 steps with additive
defaults and strict native refusal of unknown keys; keep the authored unknown
fields in the Studio patch document and show refusal rather than dropping data.

**Unresolved Apply wording:** the roadmap recommends Apply then explicit Start;
one Keel line says Apply “then restarts from step 1”. The design uses separate
Apply and Start, **no automatic playback after ACK or refusal**, pending
explicit Keel/builder reconciliation. Graph Apply can interrupt unrelated
modules; that warning stays beside Apply without a confirmation modal.

## Region-to-code integration map (not implemented here)

| Study region | Existing Studio seam / next task |
|---|---|
| Compact performance card + 10 stable ports | `apps/studio/src/canvas/ModuleCard.ts`; real Groove catalogue and port rails |
| Step cells / track & page switching | Existing sequence styles in `src/styles/sequence.css`; separate Groove-specific layout/panel (do not alter `example.step_sequencer`) |
| Exact numeric fields and segmented kind | `src/panels/widgets/NumberBox.ts`, `Segmented.ts`, and panel layout conventions |
| Authored JSON, Undo/Redo, save/open | `src/patch/PatchDoc.ts`, current app document history/persistence; unknown keys stay authored |
| Candidate Apply, refusal and actual ACK/generation | `src/engine/RealEngine.ts`, existing `loadGraph` transaction and status bridge |
| Native STOP completion, position and disconnect | **Separate ruled T2 observation implementation**, then bounded native-to-Studio status; never `FakeEngine` timer |
| Optional live status dot | `src/runtime/Ticker.ts`, driven only by genuine telemetry and visibility; rust reserved for live event/position |

S1b gate-length options and selected-step/page copy/clear/duplicate are **not**
implemented in the study and are separately labelled optional. Four visible
tracks, 64-step editor, swing, ratchets, scenes, songs, S2 live editing,
new real-time edit messages and a generic sequencer framework remain out
of scope.

## Accessibility and performance intent

Every editable button has a minimum 44px hit region; step buttons are at least
48×80px. Selected and live positions use independent outlines/labels, never
color alone. Focus is visible on buttons and numeric inputs; buttons have
pressed or disabled state and distinguish note/rest/tie textually. Status
changes use `role=status` / `aria-live`. The demo uses only event-driven
DOM updates; no `setTimeout`, `setInterval`, `requestAnimationFrame`,
websocket, network, audio or extra animation library. With reduced-motion
preference, CSS animations/transitions are disabled. No animated CSS shadows,
layout properties or simulated signal motion.

## Verification / limits (2026-11-09)

- **PASS** JavaScript parser compile of the exact committed inline script
  (`new Function` check in a V8 helper).
- **PASS** all 44 static IDs unique; all statically looked-up controls exist.
- **PASS** 7/7 controlled mock-DOM interactions: initial stopped/loaded;
  running/queued locking; dirty/undo/redo/save; applying/refusal/ACK/Start;
  off-page playhead; mute/disconnect/recovery; queued track entry/cancel.
- **PASS** source audit: no external scripts or timer-driven playhead; two
  local CSS links present; responsive 900px/600px rules and reduced-motion
  clause present.
- **PASS** real cloud Chromium browser regression: dirty and refused drafts survive
  disconnect/recovery; Start stays locked until explicit Apply ACK. Undo confirms
  the prior acknowledged snapshot was preserved. Queued STOP keeps edits locked.
- **PASS** 1280x720, 1920x1080 and 390x844: no document horizontal overflow,
  internal narrow step scrolling, page selection, visible keyboard focus, 44px
  numeric controls and no browser console/page errors. Screenshots inspected in
  workspace artifacts (not committed). Selected-step contrast and scroll focus
  were corrected after this rendered pass.
- Reproduce: `npm --prefix apps/studio ci --no-audit --no-fund`, then
  `node --test apps/studio/tests-node/groove-study.test.mjs`. Set
  `WIRESCAPE_CHROMIUM` for a nonstandard Chromium executable and optionally
  `WIRESCAPE_GROOVE_ARTIFACTS` for screenshots. The test serves only local study
  assets over loopback; this cloud policy blocks file-URL navigation.
- **NOT VERIFIED:** Safari, physical Pi/Windows display, or production Studio integration.
- **NOT VERIFIED:** real engine/Studio telemetry, Pi or Windows UI, MIDI/audio,
  native STOP/playhead, actual `loadGraph`, real persistence or physical listening.

Before merge: independent different-session source and screenshot T1
`COMMENT` review on the exact PR head; no author merge/approval.

Signed: ChatGPT (GPT-6, Groove UI study builder), 2026-11-09.

Closeout: Codex Lead (Codex, delegated study builder), 2026-11-09 America/New_York.
