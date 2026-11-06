<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape Studio roadmap — Phase 7 UI pretest, Phase 8 native integration

> **Superseded for next steps (2026-11-06).** The owner checked Phase 7 manually on the Pi. Integration with the native engine (old "Phase 8") is now planned in [kalharbi11/wirescape `docs/PLAN.md`](https://github.com/kalharbi11/wirescape/blob/main/docs/PLAN.md), Phases 1–5. This repo is imported there as `apps/studio/` (task P1A) and becomes read-only afterwards. Don't start new work here.

Updated 2026-11-05. **Phase 7 is the standalone Raspberry Pi browser/UI validation phase in this repository. Phase 8 is native-engine integration in [kalharbi11/wirescape](https://github.com/kalharbi11/wirescape).** The former “Phase 7 = real engine” numbering is superseded.

## Current integration state

| Work | Implementation / Git status |
|---|---|
| Phases 1–3 | Original browser app + FakeEngine; PR #1 is old-look and superseded by #3 |
| Phase 3.5 | Reskin / 21 modules; PR #3 merged to `main` |
| Phase 4 | Editing; PR #4 merged into `phase/3.5-reskin` |
| Phase 5 | System; PR #5 merged into `phase/4-editing` |
| Phase 6 | Signature panels/polish; PR #6 merged into `phase/5-system` |
| Phase 7 | `phase/7-pi-validation` starts from the cumulative `phase/5-system` merge result |
| Phase 8 | Planned, mainly in the separate native `wirescape` repo |

**Merge gate:** The stacked PRs #4–6 were merged into their *intermediate base branches*, not directly to `main`. At this snapshot, `main` has Phase 3.5; the cumulative Phases 4–6 are on `phase/5-system`. Before accepted Pi tests, promote that cumulative implementation to `main`, confirm hosted checks at the final SHA, and retarget/review the Phase 7 PR against `main`. Do **not** separately merge superseded old-look PR #1. This plan update does not merge code or start tests.

## Three contracts to preserve

1. **Catalogue-driven panels:** `public/wirescape_modules.json` plus optional `public/panels/<id>.json`; no module-specific branches in the generic panel builder.
2. **`EngineClient`:** the UI talks through this interface; Phase 7 continues to use `FakeEngine` for invented signal values, so there is no native host dependency.
3. **Patch v2:** IDs and unknown fields survive load/save. Native execution/output bus validation is Phase 8, not a reason to fail the Phase 7 UI-only test.

## Implemented phases

**3.5 — merge/reskin:** new kit tokens, knobs, cables, modulation UI, displays, port rails, gestures, 21 catalogue modules and tests.

**4 — editing:** undo/redo, multi-select, copy/paste/duplicate, typed values, save/import/autosave/recovery and starter patches.

**5 — system:** simulated audio/MIDI/OSC status/settings/faults, panic, automatic Pi profile and update-check stub.

**6 — signature panels and polish:** Step Sequencer, Macro Oscillator, Mixer/Output, six Qt starter fixtures alongside browser demos, history/keyboard/modal/telemetry corrections. CHAOS-lite remains generic. **None of these phases has been certified on the physical Pi yet.**

## Phase 7 — Pi UI pretest (NEXT)

**Repository:** `Wirescape-UI`. **Branch:** `phase/7-pi-validation`. **Hardware runner:** dedicated `pi-ui` self-hosted Pi runner; headed *system* Chromium and actual monitor. **Engine:** `FakeEngine` only. **Objective:** find/fix UI, browser, physical display, performance, interaction and reliability issues *before* moving UI to the native engine repo.

| Gate | Required work |
|---|---|
| **7.0 Integration/runner readiness** | Cumulative Phases 3.5–6 and acceptance harness on `main`; exact-SHA hosted build/typecheck/Playwright green; runner online, display session accessible. A queued/cancelled job or one with zero setup steps is infrastructure **BLOCKED**, not UI failed. |
| **7.1 Fix test-harness gaps before hardware sign-off** | Replace the current 120-rAF-frame Pi sample with sustained 15–30-second post-warmup intervals; record p95/p99 pacing/stalls in addition to mean FPS. Keep 20-module/40-cable modulation stress fixture and **add** a representative signal-bearing FakeEngine VCO/Mixer/Output fixture with active meters/scope. Check the 5-second auto-Pi-profile watchdog separately; collect renderer and read-only thermal/load context. Adjust test/job timeouts if needed. |
| **7.2 Automated headed Pi regression** | Run the current full browser suite with no Xvfb/headless fallback: 21 modules; all patch gestures; 50/100/185% zoom; save/restore/unknown fields; editing/undo; keyboard/modals; fake system faults; all 12 starters at mouse and touch-profile density. Capture screenshots, video, traces and errors. |
| **7.3 Physical performance** | Normal/Pi profile × fit/100% view, modulation-heavy and signal-bearing workloads; target ≥30 measured rAF FPS on Pi with acceptable frame pacing. Record actual viewport/resolution/DPR/refresh, GPU/backend, Chromium, load/temperature and visibility/focus. A 30Hz ticker update cap is **not** proof of 30 FPS. |
| **7.4 Owner usability checkpoint** | Manually check patching, mapping/cable amount/colours, typed controls, keyboard, focus, undo/redo, save/reload, signatures and large panels on the real display; verify cards/port labels/readability/hit targets. Check touch-profile geometry, but claim physical touch only if a touchscreen is actually used. Decide Q-B cable drop behavior (replace normally, Shift-add currently; plan default is add/sum), including an accessible touch action. |
| **7.5 Fix, rerun, sign off** | Fix only UI-related defects here, add regressions and rerun on exact final merged SHA. Document hardware measurements in `docs/PERFORMANCE.md` and decisions in `DESIGN.md` / `DECISIONS.md`. Close OPEN-5 only with valid physical evidence. Label runner/display failures, fake-engine limitations and Phase 8 dependencies separately. |

**Phase 7 exit:** hosted checks green; complete headed physical Pi evidence at final SHA; ≥30 actual UI FPS with acceptable pacing; no unresolved UI-only blocker; documented user interactions and owner decisions. **Out of scope:** audible sound, real MIDI/OSC/audio devices, DSP equivalence, native patch execution, realtime contention, engine transport.

Execution details: [Phase 7 test plan](PI_UI_TEST_PLAN.md), [dedicated Pi runner](PI_UI_RUNNER.md), and [pre-Pi code review notes](PRE_PI_READINESS_REVIEW.md). The existing workflow remains **manual, owner-only and `main`-only**. No hardware run is triggered by changing these documents.

### Review items from Phases 3.5–6 to address before/during the first Pi run

- **Measurement blockers:** 120-frame sample is too short, no sustained stutter/thermal context, forced profiles do not test the automatic fallback; stress patch has LFOs/VCFs but no source-to-Output signal path to animate meters/scope.
- **UX decisions/tests:** Q-B replace vs sum/add on cable drop (especially touch access); large Sequencer/Macro, Mixer/Output meters, side-rail jacks and all starter layouts at actual scale; history/keyboard/modal-focusing regression coverage on real Pi.
- **Document as FakeEngine limitation, not Pi defect:** Sequencer Note/Tie UI drawing exists, but simplified FakeEngine gate semantics do not validate real musical sustain.
- **Keep in Phase 8:** patch-v2 native codec/output routes, asynchronous engine refusals/reconciliation, genuine telemetry, CC mapping, realtime audio/DSP and device tests.

## Phase 8 — native Wirescape integration

**Repository:** primarily `kalharbi11/wirescape`; begins after Phase 7 UI acceptance (planning may run in parallel). Full technical handoff: [PHASE8_HANDOFF.md](PHASE8_HANDOFF.md).

1. Pin native revision; implement lossless patch-v2 adapter, module metadata/output buses and protocol acknowledgement/refusal.
2. Spike loopback-only token-protected headless C++ host + `RealEngine.ts`, initial real VCO → Output, one cable/knob, user-approved real sound.
3. Add safe telemetry snapshots, status, real MIDI/OSC, modulation parity and controlled graph replacements outside audio callback.
4. Re-run Pi/desktop fixtures with real engine/audio contention; then packaging, kiosk and `apps/studio` import / Qt retirement as separately reviewed gates.

## Pending owner decisions

- **Q-A:** record the Pi display's *actual* resolution, DPR and input hardware; do not infer “2K” exact pixels.
- **Q-B:** default proposed sum/add for multiple cables; current ordinary drop replaces while Shift-drop adds. Do not change silently.
- **OPEN-1/2/3/4/6/7:** preserve existing design defaults unless actual Pi usability exposes a problem.
- **OPEN-5:** physical Pi FPS/frame pacing unmeasured; keep open until Phase 7 evidence exists.
- **Q-C:** dark theme later.

## Working rules

One branch/PR per phase; keep the library/styles consistent with `DESIGN.md` and `DECISIONS.md`. Do not commit binary screenshots or recordings; use CI artifacts. No native runner/service reconfiguration or real devices in Phase 7. Report unavailable stages honestly, not as green.
