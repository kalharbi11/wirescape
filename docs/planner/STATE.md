<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under60lines. History lives in git.

**Updated:**2026-11-06 by Codex Lead. **Pi first; Phase2 runs beside Test Session1. Any owner failure in #271 preempts all work.**

## Current evidence and in-flight work
- Main includes #285 Phase2 remaining plan and #263 modules; Test Session1 #271 is deferred, no owner failure/result posted yet. Table records conditional H/I/J; no hardware pass claimed.
- #273 live-edit sidecar: Keel Medium fixed at `00a5b24`, exact-head local acceptance PASS; `ci/pi` and Keel T2 review requested.
- #274 bench summary: required semantics/docs/task at `c6ed064`, headless build + sidecar acceptance PASS; `ci/pi` and Keel T2 review requested.
- #279 patch folder: independent T1 review and own Web/ARM64/sanitizer checks PASS; auto-merge enabled. Required Windows live-replan test failure tracked #297; no check waived.
- #283 on-screen keyboard: separate-session T1 no-findings review, Web/full Electron and original required CI green at `29a5ed2`; ready + merge gate submitted.
- #282 autosave fixed at `5be3b59`: browser fixture/typecheck errors fixed, eight focused tests PASS, separate-session review done; final Web/ARM64/acceptance PASS, sanitizers pending.
- #284 soak harness fixed at `e2bb430`: malformed hello no longer leaks child, nine unit tests PASS, separate-session review done; final ARM64/task acceptance PASS, remaining CI pending.
- #297 required live-replan CI blocker: deadlines3blocks/gap20ms unchanged; yield-scheduler experiment still fails under CPU contention. Stopped for Keel, no production fix or pass claimed.
- Phase1 sign-off still needs #256/#250 conformance, #271 owner results and contention evidence. Checkpoints do not stop independent work.

## Phase2 queue (issues from every P2_REMAINING row)
- #287 2B-b MIDI Learn: blocked on #273; existing CC observation seam must be verified before any new contract.
- #288 2B-c MIDI hotplug: active, draft #298 at `366867a`; source identity/choke ordering + fake regressions, no session.cpp overlap.
- #289 2B-d keyboard default: implemented at `1141532`,23 targeted browser tests PASS; stacked on #283, independent review/full Web pending.
- #290 2B-e: continuing #210/#163; safe synthetic-MIDI soak slice active. Keel/Compass ruling requested for stale Qt writeback/last-note/Learn semantics; no invented protocol.
- #291 2C-a audio loss: active fake-backend implementation; bounded retries, remembered identity, no fallback. T2 Keel review required.
- #292 2C-b honest callback load/dropouts: blocked on #274.
- #293 2C-c device details/optional settings: blocked on #291.
- #294 2D-c patch browser: blocked on #279+#282.
- #295 2D-d crash recovery offer: blocked on #282.
- #296 2F-b60min Pi soak: blocked on #284; performance metrics require #274; private pi5 runner only.
- Full briefs/acceptance/size/file ownership are in each issue. Start dependency lanes as their prerequisite merges.

## Lead routing and retained rulings
- Keel: T2 exact-head reviews #273/#274/#288/#291 and rule on #297/#290; never substitute a GPT review for cross-family review.
- Compass: scope/plan questions; TrackS1 has its own builder. No OSC3D, telemetry3A or Windows#266 started here.
- Electron main is the sole automatic restarter; renderer requests never reset crash-loop budget.
- D3: imported docs roundtrip unchanged; execution defaults live only in engine copy.
- D5/D7: new loadGraph uses D5; eligible live edits preserve ownership, bounded busy retry; non-rendering goes directly to D5.
- Benchmark stderr fields are render walltime; late catchup blocks each count as overruns. Missing metrics never means zero.
- Known optional Cortex-M7/#159 red and informational static are distinct from required failures.
- Tests/CI never open real audio/MIDI or LAN. Pi evidence through pi5 runner only; no SSH/laptop/device access.
- Owner controls hardware sitting/checkpoint jam when available. Nothing requested now.

Signed: Codex Lead (Codex),2026-11-06
