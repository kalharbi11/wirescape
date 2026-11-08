<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-08 by Codex Lead; retains Compass/Keel rulings.
Pi first. Phase 2 and modules run beside owner Test Session 1; any #271 failure preempts other work.

## Delegation (new, AGENTS.md 2026-11-08)
- Codex Lead now merges stuck-but-green reviewed PRs, merges past inherited static/cortex-m7 reds,
  applies Claude conditional approvals once their evidence is posted, decides Pi need for module/UI PRs,
  and writes `Protected path named:` lines for docs/tasks/** and STATE.md. Modules and their DSP are T1.
- Claude is needed only for real T2 (engine2 core, protocol/spec, patch format, ABI/SDK headers, CI/merge
  control) and plan/scope changes. Batch them: one `Keel: T2 batch` comment, so one session clears them all.

## Done this session (11-08)
- Merged #367 (shared CPU probe). Closed #181/#180 (superseded by docs/SDK_AUTHOR_GUIDE.md); closed #374 (folded in here).
- Approved and marked ready: #371 (emit rename), #301 (audio recovery; Keel pre-approval applied on the pi5 head),
  #368 (A2 telemetry; Keel T2 clean, one Low). The gate merges them once windows-x64 is green.
- Ruling: the #368 Pi 64-tap/no-tap comparison moves into #292 (callback load); it doesn't block A2.

## Telemetry #350
- After #368 merges: #369 A3 wasm retargets to main, then final native/wasm/site evidence (T2: Keel batch).
- #359 B-wire follows. Owner telemetry visual check waits for B-wire, then a combined Electron/web check.
- Keep: reset event cursors on every commit; first post-edit burst must pulse; never fake phase.

## Modules (all T1 now; Codex Lead owns)
- #304 Spectral: conflicts with merged #367. Drop its private probe copy, use the shared tools/module_cpu_probe, rerun pi5/accept.
- #373/#372: main integrated at 852a1ce (two parents, no rewrite); Python 8/8 pass; final CI/review pending.
- #348 reverb, #345 delay, #344 resonator (--channels 8 after #373), #323 quantizer: switch to the shared probe,
  run Pi CPU acceptance, then Codex Lead merges.
- #346 oscillator UI: rebase conflict (dirty), then merge (reviews clean).
- #310 Spectral Interleave/Vocoder: stacked on #304.

## Phase 2 engine queue
- #301 merged (aba3042). Next: #293 device routes, #292 callback load/dropouts (+ tap-cost check), #296 fake-device soak.
- #290/#210 controls: under the existing Keel Nov7 ruling; #210 has pi5/accept on d715bb4, needs review + merge.
- #287 Learn: blocked for raw-CC observation contract (status.midi.activity lacks CC identity/value). T2: Keel batch.
- #82 J5 OSC (phase 3, T2): parked until Phase 3; it needs a Claude review then.

## Owner-requested three-agent handoff (#376)
- Paste-ready lanes: docs/tasks/BUILD_LEAD_2026_11_08.md; three chats, maximum three active items each.
- Codex Lead pushed #373 integration and #292 implementation (#378, 1ac6d71); checks/reviews pending.

## Retained rules
- Tests/CI never open audio/MIDI/LAN. No agent SSH to Pi/laptop; private runner supplies Pi evidence.
- Missing metrics never count as zero; queued or prior-head results never count as current evidence.
- Required checks: guard, linux-arm64, windows-x64, sanitizers. static is diff-scoped and often red from inherited code.
- Keep documents lossless, Electron main sole automatic restarter, unknown data preserved.
- Don't poll CI. Gates and runner do the waiting. No weakening tests/thresholds.
