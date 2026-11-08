<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 America/New_York by Codex Lead; verified live PR heads, reviews, CI and merge records.
Pi first. Phase 2 and modules run beside owner Test Session 1; any #271 failure preempts other work.

## Lead split and priorities
- Compass owns phase/product priorities and module lane; Keel owns T2 technical review/rulings.
- Codex Lead owns builder queue, scoped takeovers and T0/T1 gate approvals.
- Existing owner-requested schedules unchanged; no additional polling/schedules.
- All Phase 1 code merged; #271 owner results remain before Checkpoint A. Minimum: 0, A, B, C, D3.
- Windows #266 follows Test Session 1. Web demo remains an independent showcase lane.

## Confirmed merged
- #161 Plaits/Cortex context and #343 Macro gate merged. Keel waived #161 Pi rerun.
- #331 patch browser/save safety, #352 waveform tail, #353 30Hz cap, #364 LFO phase merged.
- #362 engine2 telemetry taps merged at bf74743; A2/A3 builders are unblocked.

## Telemetry #350
- Contract: docs/design/ENGINE_TELEMETRY.md; shared brief: docs/tasks/T350_ENGINE_TELEMETRY.md.
- #363 A0 snapshots 4e37373 merged; Studio snapshot semantics are on main.
- #368 A2 e281706: hosted functional checks green; Keel T2 review requested, ci/pi added; 64-tap/no-tap Pi command/evidence missing.
- #369 A3 wasm 8cf7c46: stacked on #368, source preflight clean; retarget main after A2 merges, then final native/wasm/site evidence.
- Critical host regression: reset event-count cursors on every graph commit; first post-edit burst must pulse.
- #359 B-wire follows host merge; B-mod is already merged. Do not fabricate phase.
- Owner telemetry visual check waits for B-wire, then batch Electron/web check.

## Shared CPU evidence and modules
- #367 shared probe extracted; review found channel0-only/NaN masking. Repaired to scan all channels/count nonfinite faults.
- #367 current signed branch handoff is authoritative: full block-count/duration checks, named output bounds and authored static fixes pushed.
- 7 Python tests and strict local C++17 output regression pass. Independent follow-up/fresh hosted checks required; no Pi timing claim.
- After #367 merges, integrate probe into effects and Spectral without duplicate targets/helpers; run specified Pi timing.
- #348 reverb be3c306: freeze precision fix independently reviewed; hosted checks pass, Pi CPU evidence still required.
- #304 Spectral 38b2c24: hop-aligned :32 stress and integration fixed; hosted native/Web gates green; exact-head pi5 and pi5/accept success posted.
- #304 Pi attempt2 shares old poll run URL; verify commit status/head, not the old-head handoff. Worst stress 1.023ms/2.667ms period.
- #345 Delay 99b7eaf, #344 Resonator 06d5ede, #323 Quantizer a65a70a: verify current fixes/reviews/acceptance; do not repeat old findings blindly.
- #346 oscillator UI 48aec95: hosted checks green, independent reviews clean; author Self-review checklist requested before approval.

## Phase 2 engine queue
- #301 audio recovery aba3042: authorized CMake repair present; ARM64/sanitizers/task acceptance green, no pi5 status yet.
- Private Pi poll attempt2 is running; do not start duplicates. Compass/Keel can approve under Keel's pre-approval once exact-head pi5 passes.
- #293 device routes/details follows #301. #292 callback load/dropouts and #296 fake-device soak remain available.
- #290/#210 controls: implement under existing Keel Nov7 ruling.
- #287 Learn: blocked for raw-CC observation contract; status.midi.activity scalars lack CC identity/value.

## Retained rules
- Tests/CI never open audio/MIDI/LAN. No agent SSH to Pi/laptop; private runner supplies Pi evidence.
- Missing metrics never count as zero; queued or prior-head results never count as current evidence.
- Keep documents lossless, Electron main sole automatic restarter, unknown data preserved.
- Review exact changed seams; don't poll CI. Gates and runner do waiting. No weakening tests/thresholds.
