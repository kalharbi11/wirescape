<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-08 by Keel/Compass (Claude); lead batch from #376 cleared.
Pi first. Phase 2 and modules run beside owner Test Session 1; any #271 failure preempts other work.

## Delegation (new, AGENTS.md 2026-11-08)
- Codex Lead now merges stuck-but-green reviewed PRs, merges past inherited static/cortex-m7 reds,
  applies Claude conditional approvals once their evidence is posted, decides Pi need for module/UI PRs,
  and writes `Protected path named:` lines for docs/tasks/** and STATE.md. Modules and their DSP are T1.
- Claude is needed only for real T2 (engine2 core, protocol/spec, patch format, ABI/SDK headers, CI/merge
  control) and plan/scope changes. Batch them: one `Keel: T2 batch` comment, so one session clears them all.

## Done this session (11-08)
- Merged #367 (shared CPU probe). Closed #181/#180 (superseded by docs/SDK_AUTHOR_GUIDE.md); closed #374 (folded in here).
- #301 audio recovery and #371 emit rename merged. Codex Lead merged approved #368 at963384a; required checks/Pi pass.
- Ruling: the #368 Pi 64-tap/no-tap comparison moves into #292 (callback load); it doesn't block A2.

## Telemetry #350
- #369 A3: static coverage red = tooling gap, fixed in #380 (multi-build static). Codex Lead may merge #369 past that
  one static red once required checks, web-demo engine and the T1 review are green on the exact head.
- #359 sidecar start gate is met by #368 merge; owner combined Electron/web visual check waits for B-wire.
- Keep: reset event cursors on every commit; first post-edit burst must pulse; never fake phase.

## Modules (all T1 now; Codex Lead owns)
- #304 Spectral: conflicts with merged #367. Drop its private probe copy, use the shared tools/module_cpu_probe, rerun pi5/accept.
- #373 at852a1ce: independent review and native/acceptance pass; Windows requested; static tool-download504 remains.
- #348/#345 shared-probe integrations pushed; #344 waits for #373 poly probe. Keep module-specific exact-head Pi evidence.
- Delay Web passed158 browser tests then Electron download504; one retry. Reverb/Resonator CodeQL download504 retried once.
- #346 integrated main ataf01a60; local351-case DSP matrix/tracking/LFO checks pass; conflict review/new CI pending.
- #310 Spectral Interleave/Vocoder: stacked on #304.

## Phase 2 engine queue
- #293 routes is available after #301. #378 (#292): Keel T2 review at 23494de, no High. Builder adds the ruled tap gate
  (median paired ratio <= 1.05, fail-closed) to --telemetry-bench; then exact-head pi5/accept + green required checks.
- #296 unblocked by Keel ruling: new ci/pi-edit-soak path in wirescape-pi bridge.py (own 3900 s timeout); public
  harness adds --bench-summary + faults parsing first (separate PR, same issue). No generic timeout change.
- #290/#210 controls: under the existing Keel Nov7 ruling; #210 has pi5/accept on d715bb4, needs review + merge.
- #287 Learn UI waits on #381 (T2 seam: midiLearn command + status.midi.learn, latest-wins, transient arming).
- Compass 11-08: no scope change. #271 stays owner-only Phase 1 sign-off. Phase 2 checkpoint is ready only when
  #378, #369, #380, #381/#287 and #296 (exact-head 3600 s pi5/edit-soak) are merged with evidence.
- #82 J5 OSC (phase 3, T2): parked until Phase 3; it needs a Claude review then.

## Owner-requested three-agent handoff (#376)
- Paste-ready lanes: docs/tasks/BUILD_LEAD_2026_11_08.md; three chats, maximum three active items each.
- Three chats keep at most three active items each; current handoffs and contract batch are on #376.

## Retained rules
- Tests/CI never open audio/MIDI/LAN. No agent SSH to Pi/laptop; private runner supplies Pi evidence.
- Missing metrics never count as zero; queued or prior-head results never count as current evidence.
- Required checks: guard, linux-arm64, windows-x64, sanitizers. static is diff-scoped and often red from inherited code.
- Keep documents lossless, Electron main sole automatic restarter, unknown data preserved.
- Don't poll CI. Gates and runner do the waiting. No weakening tests/thresholds.
