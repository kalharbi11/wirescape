<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-08 by Codex Lead. Pi first. Phase 2 and modules run beside Test Session 1; any owner failure in #271 preempts other work.

## Lead split
- Compass (platform): app, engine lanes, web demo, roadmap. Compass (modules): Track S and module lane.
- Codex Lead: builder queue, T0/T1 review/gate approvals; Keel: T2 cross-family review and technical stops.
- Existing owner-requested check-ins unchanged. Do not create more schedules.

## Sign-off and priorities
- All Phase 1 code is merged. Only owner Test Session 1 #271 remains before Checkpoint A.
- Minimum owner session: #271 sections 0, A, B, C and D3; no hardware pass claimed.
- Priority: owner failures, Phase 2 critical path, modules in parallel. Windows #266 follows Test Session 1.

## Engine and app queue
- #301/#291 audio recovery: both Mediums repaired at 003f64e with fake-device regressions; fresh CI, Keel and Pi required. #293 follows.
- #331/#294 patch browser: takeover fixing preload/reset Lows plus malformed-load Save-target hazard; fresh Web/Electron evidence required.
- #292 callback load/dropouts: ready builder brief in issue. #296 fake-device Pi soak: unblocked, status/todo, exact brief posted.
- #287 MIDI Learn: blocked for Keel correction; status.midi.activity is recent endpoint activity, not CC number/channel/value.
- #290/#210 MIDI controls: Keel's Nov7 ruling exists; implement last_note/min/max within that ruling (brief posted). Not waiting on a new general contract.
- #293 device details/routes: exact dependency-aware brief posted; do not duplicate #301 recovery.

## Engine telemetry #350
- Authority: docs/design/ENGINE_TELEMETRY.md and docs/tasks/T350_ENGINE_TELEMETRY.md.
- #364 B-mod LFO phase: approval withdrawn/draft blocked; mandatory saved-patch bit-identical render case missing; regression being added.
- #352 waveform tail: reviewed; acceptance metadata fixed at 6bae69b; fresh CI before approval.
- #353 30Hz cap: cadence-only T1, Refs #350, old proposal superseded at 3a5ec95; fresh CI before approval.
- #363 A0 snapshots: blocked on Web restoration regression and acceptance browser bootstrap; exact review posted.
- #362 A1 taps dc8c1d1: corrected reconfigure High; Keel exact-head T2 review required.
- After A1 merges: #357 A2 sidecar/spec (T2) and #358 A3 wasm (T1) parallel; then #359 B-wire after host and phase prerequisites.

## Module lane
- #348 reverb: precision fix + unchanged-tolerance regression pushed be3c306 and independently checked; dedicated tests pass, fresh CI/Pi pending.
- #304 Spectral/shared CPU probe: review requires hop-aligned mode stress (:32, not :4), Pi evidence and current integration.
- Builder brief: extract generic CPU probe/tooling as an independent T1 PR to unblock effects' Pi acceptance.
- #345 delay and #344 resonator still need exact-head Pi CPU evidence; #344 independent module review outstanding.
- #323 quantizer: prior Mediums remain (Custom visibleWhen, task title/acceptance metadata); concrete brief posted.
- #346 oscillator UI: formatting follow-up 48aec95 independently checked; fresh CI before approval. #343 Macro gate is T2, Keel review requested.

## Plaits and CI
- #159/#161 header-context fix already passes actual Cortex all modules/both variants, plus native/Web checks at 39eb93d.
- Raw REST says mergeable=true; normalized connector false is misleading. No speculative conflict edits.
- #161 needs Keel T2 review and exact-head pi5/accept for compiled adapter changes; missing ci/pi-accept added.
- #361 static diagnostic ownership is separate; protected tooling authorization requested, preserve first-party gating and upstream reports.
- Known cortex baseline #159 is non-required; static is informational. Never weaken checks or treat queued work as evidence.

## Retained rules
- No agent SSH to Pi/laptop; Pi evidence only via private runner. Tests use null/fake devices.
- Electron main alone restarts. Documents round-trip losslessly. Missing metrics never count as zero.
- Web demo is a separate showcase lane (D14), not a product stage. Don't poll CI; gate/runner do the waiting.
