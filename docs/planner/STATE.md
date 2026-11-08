<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 America/New_York by Codex Lead, verified against live PRs and Keel comments. Pi first; owner #271 failures preempt.

## Lead split
- Compass (platform): app, engine lanes, web demo, roadmap. Compass (modules): Track S and module lane.
- Codex Lead: builder queue, T0/T1 review/gate approvals; Keel: T2 cross-family review and technical stops.
- Existing owner-requested check-ins unchanged. Do not create more schedules.

## Sign-off and priorities
- All Phase 1 code is merged. Only owner Test Session 1 #271 remains before Checkpoint A.
- Minimum owner session: #271 sections 0, A, B, C and D3; no hardware pass claimed.
- Priority: owner failures, Phase 2 critical path, modules in parallel. Windows #266 follows Test Session 1.

## Engine and app queue
- #301/#291: Keel confirmed Medium fixes; generated-header CMake build fix pushed aba3042. Fresh CI/pi5, then Compass/Keel approval under his pre-approval.
- #331/#294 patch browser: 1d154c3 reviews and exact-head hosted acceptance green; Codex gate approval posted.
- #292 callback load/dropouts: ready builder brief in issue. #296 fake-device Pi soak: unblocked, status/todo, exact brief posted.
- #287 MIDI Learn: blocked for Keel correction; status.midi.activity is recent endpoint activity, not CC number/channel/value.
- #290/#210 MIDI controls: Keel's Nov7 ruling exists; implement last_note/min/max within that ruling (brief posted). Not waiting on a new general contract.
- #293 device details/routes: exact dependency-aware brief posted; do not duplicate #301 recovery.

## Engine telemetry #350
- Authority: docs/design/ENGINE_TELEMETRY.md and docs/tasks/T350_ENGINE_TELEMETRY.md.
- #364 B-mod phase: 7f6514b saved-patch regression + hosted checks green; processing replacement gate approval.
- #352 waveform tail: 6bae69b exact-head hosted checks green; Codex gate approval posted.
- #353 30Hz cap: 3a5ec95 exact-head hosted checks green; Codex gate approval posted.
- #363 A0 snapshots: blocked on Web restoration regression and acceptance browser bootstrap; exact review posted.
- #362 A1 taps 9f1f3c0: Keel approved; gate waits current sanitizer run. A2/A3 begin after merge.
- After A1 merges: #357 A2 sidecar/spec (T2) and #358 A3 wasm (T1) parallel; then #359 B-wire after host and phase prerequisites.

## Module lane
- #348 reverb be3c306: precision repair independently reviewed and hosted acceptance green; shared probe/Pi timing still required.
- #304 Spectral/shared CPU probe: review requires hop-aligned mode stress (:32, not :4), Pi evidence and current integration.
- Builder brief: extract generic CPU probe/tooling as an independent T1 PR to unblock effects' Pi acceptance.
- #345 delay and #344 resonator still need exact-head Pi CPU evidence; #344 independent module review outstanding.
- #323 quantizer: prior Mediums remain (Custom visibleWhen, task title/acceptance metadata); concrete brief posted.
- #346 oscillator UI: formatting 48aec95 independently checked. #343 fb4f4b1 Keel-approved; merge #161 first, then reconcile main.

## Plaits and CI
- #159/#161 header-context fix already passes actual Cortex all modules/both variants, plus native/Web checks at 39eb93d.
- Raw REST says mergeable=true; normalized connector false is misleading. No speculative conflict edits.
- #161: Keel approved 39eb93d and waived Pi rerun (equivalent Linux code + hosted ARM64 Macro tests). Merge before #343.
- #361 static diagnostic ownership is separate; protected tooling authorization requested, preserve first-party gating and upstream reports.
- Known cortex baseline #159 is non-required; static is informational. Never weaken checks or treat queued work as evidence.

## Retained rules
- No agent SSH to Pi/laptop; Pi evidence only via private runner. Tests use null/fake devices.
- Electron main alone restarts. Documents round-trip losslessly. Missing metrics never count as zero.
- Web demo is a separate showcase lane (D14), not a product stage. Don't poll CI; gate/runner do the waiting.
