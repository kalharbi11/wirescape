<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09, Compass decisions on top; Codex Lead queue below.
Windows-first setup, Pi performance target; owner failures preempt. No stale result passes.

## Compass 2026-11-09: D15-D19 in docs/PLAN.md; sequence in docs/planner/CLEANUP.md
- Qt is retired now (#437); no Qt fixes. Order: R1 Qt-free build, R2 fixtures onto codec, R3 delete.
- Top: Windows bootstrap #436 W1, then Pi #444-#446. Finish before start (10+ open PRs: no new).
## Owner priorities and latest Compass/Keel
- Interleave higher priority than Vocoder. OSC now active independent work.
- MIDI seam/controls can progress; LearnUI waits actual seam merge.
- Existing agents keep16step405/410and Groove430; no duplicate sequencer claims.
- Compass/Keel431review approved stopped-edit playable Groove first, live editsS2.
- Keel430binding: sequencing.groove separate ID; versioned JSON config -> fixed
  4track/64step POD quiescent adapter; fail-closed, unknown editable data preserved.
- S0mayproceed independently. S1release-ready requires stable16step/combined regressions.
- Existing params/inputs only, no new protocol/ABI; load stopped, release on replacement,
  step1restart, bounded2MiB. Shared helper extraction needs a real second use.

## Actual implemented / pending
- Reverb348MERGED c39af7b; catalogue26. Delay/Spectral/Blend/Resonator retained.
- Telemetry379MERGED07fb277; eight curated patches412MERGED3af48ae.
- Quantizer323prepared0acdad4 with27modules; independent review no High/Medium.
  Exact native/task/Windows/sanitizers/Studio/WASM PASS; static had2authored
  assertion-wrap errors + inherited keyboard parser. Root fixed ONLY wraps385ad20.
  Fresh checks/integration required; not merged or a second DSP implementation.
- 427public patch folder/design guide is NOT merged. Final main reconciliation/gates
  remain despite successful source UI/WASM evidence. Preserve owner fast-lane rule.
- 398render evidence has independent T1 and780e280fiveCTest/Pi PASS; task-only
  cap repair3f12baa needs final gates. 296actual3600s private edit soak still missing.

## New-agent queue (claim before editing; continue branches)
- Canonical: docs/tasks/CLAIMABLE_2026_11_09.md; index393 refreshed.
- READY305/310Interleave-first: main retarget/integration, phase/chunks/state/WASM/Pi.
  Keep ASan tail fix and actual hop-boundary stress; Vocoder polish follows.
- READY381/390 +290/210: reconcile main, fake/native/UMP/poly/CC evidence.
  Keel final helper-refactor look;287UI after actual381merge. No duplicate seam.
- READY72/82OSC: current-host inventory/task first, reuse portable services/tests;
  no staleQt feature merge. Keel new host ownership/protocol decision before invention.
- READY385/398 ->386 +296: render integrity/provenance and actual private soak.
- READY307/400: repair authored79+233format diagnostics, then exact T2 Claude review.
- READY426/427: existing rollout branch reconciliation and final selected acceptance.
- Audio399and Notes423have bounded task fixes; semantic contracts remain Keel stops.

## Pi preparation (owner authorized; installation after Windows test)
- READY #445 preflight/logs: docs/tasks/PI_SOURCE_PREFLIGHT.md; no auto-install.
- READY #446 ARM64 Electron/profile smoke: docs/tasks/PI_APP_SMOKE.md; reuse #386.
- Queue/evidence: #444, docs/tasks/PI_SETUP_QUEUE.md; parent #436, related #235.
- Pi nightly42560014392 pending; actual source SHA/results required, not green.
- #101 baseline pending; #274 display/#296 soak/#271 hardware stay separate.

## Permanent gates
- Required guard/Linux/Windows/sanitizers PLUS task/Studio/WASM/Pi as applicable.
- T1different-session review; T2cross-family. Keep allIDs/unknown patch data.
- No force-push/main write, devices/SSH/LAN, suppression or threshold weakening.
- Electron sole automatic restarter; defaultlocalhost/offLAN; no invented wirefields.
- Update patch-design limitations in capability integration PRs per owner mandate.
- No CI polling/schedules. Handoff exact head, real evidence and remaining blockers.
