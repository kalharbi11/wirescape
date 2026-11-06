<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-06 by Codex Lead. **Pi first; Phase 2 runs beside Test Session 1. Any owner failure in #271 preempts all work.**

## Current evidence and in-flight work
- Main includes #285 Phase2 remaining plan and #263 modules; Test Session1 #271 is deferred, no owner failure/result posted yet. Table records conditional H/I/J; no hardware pass claimed.
- #273 live-edit sidecar: Keel Medium fixed at `00a5b24`, exact-head local acceptance PASS; `ci/pi` and Keel T2 review requested.
- #274 bench summary: required semantics/docs/task at `c6ed064`, headless build + sidecar acceptance PASS; `ci/pi` and Keel T2 review requested.
- #279 patch folder: independent T1 review and own Web/ARM64/sanitizer checks PASS; auto-merge enabled. Required Windows live-replan test failure tracked #297; no check waived.
- #283 on-screen keyboard: separate-session T1 no-findings review, Web/full Electron and original required CI green at `29a5ed2`; ready + merge gate submitted.
- #282 autosave fixed at `5be3b59`: browser fixture/typecheck errors fixed, eight focused tests PASS, separate-session review done; final Web/ARM64/acceptance PASS, sanitizers pending.
- #284 soak harness fixed at `e2bb430`: malformed hello no longer leaks child; nine tests, independent review and required ARM64/sanitizer/task acceptance PASS. Ready; merge gate submitted, new readiness checks must pass.
- #297 / draft #299 (`d878e8e`): required live-replan CI blocker; 3-block/20ms limits unchanged. Scheduling experiment failed twice under contention; stopped for Keel, no production fix/pass claimed.
- Phase 1 sign-off still needs #256/#250 conformance, #271 owner results and contention evidence. Checkpoints do not stop independent work.

## Phase 2 queue (issues from every P2_REMAINING row)
- #287 2B-b MIDI Learn: blocked on #273; existing CC observation seam must be verified before any new contract.
- #288 2B-c MIDI hotplug: active, draft #298 at `97dcd6a`; identity/choke-order fixes, build + 11 tests PASS. Keel and pi5 requested; no session.cpp overlap.
- #289 / #300 keyboard default: `1141532`, 23 targeted tests PASS plus independent review/14 tests PASS; stacked on #283, full Web after retarget pending.
- #290 2B-e / #210: safe slice `d715bb4`, 5 MIDI suites + paced 60s synthetic soak PASS and independent test-slice review. Full feature blocked on Keel/Compass writeback/last-note/Learn ruling.
- #291 / #301 audio loss: `43101f8`, build + 4 fake audio/sidecar tests PASS; remembered identity, bounded retry, no fallback. Keel/ci/pi requested.
- #292 2C-b honest callback load/dropouts: blocked on #274.
- #293 2C-c device details/optional settings: blocked on #291.
- #294 2D-c patch browser: blocked on #279+#282.
- #295 2D-d crash recovery offer: blocked on #282.
- #296 2F-b 60-minute Pi soak: blocked on #284; performance metrics require #274; private pi5 runner only.
- Full briefs/acceptance/size/file ownership are in each issue. Start dependency lanes as their prerequisite merges.

## Lead routing and retained rulings
- Keel: T2 exact-head reviews #273/#274/#288/#291 and rule on #297/#290; never substitute a GPT review for cross-family review.
- Compass: scope/plan questions; Track S1 has its own builder. No OSC 3D, telemetry 3A or Windows #266 started here.
- Electron main is the sole automatic restarter; renderer requests never reset crash-loop budget.
- D3: imported docs roundtrip unchanged; execution defaults live only in engine copy.
- D5/D7: new loadGraph uses D5; eligible live edits preserve ownership, bounded busy retry; non-rendering goes directly to D5.
- Benchmark stderr fields are render walltime; late catchup blocks each count as overruns. Missing metrics never means zero.
- Known optional Cortex-M7/#159 red and informational static are distinct from required failures.
- Tests/CI never open real audio/MIDI or LAN. Pi evidence through pi5 runner only; no SSH/laptop/device access.
- Owner controls hardware sitting/checkpoint jam when available. Nothing requested now.

Signed: Codex Lead (Codex), 2026-11-06
