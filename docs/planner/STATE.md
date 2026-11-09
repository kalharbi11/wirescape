<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09, Codex Lead review outside #439/Pi installation.
Pi first; #271 hardware failures preempt features. No queued result is a pass.

## Groove S1 first
- #451 native S1(a) stays with existing builder; child #454 owns acceptance.
- Lead31e0aa8 fixes controls0..9; catalogue/conformance now PASS. Linux/Windows
  fail102cycles: nominal fixture must drain egress between blocks, not raise caps.
- Mandatory saturated NOTE_OFF retry/reload-pending + note identity tests missing;
  add separate deliberate saturation fixture, format, rerun accept454/Pi/T1.
- S1(b) functional editor waits designer + Keel authoritative completed STOP/
  playhead contract (#4326732415072); no inferred state/invented wire fields.
- Keep v1, 4x64 transient POD, stable IDs, stopped Apply.

## Other reviewed agent work
- #405960ec61 native16step -> #410e151680 UI: existing owner, preserve curated
  exports/default8/statev1; fix cppcheck emit macro + tests.cpp:376 formatting,
  fresh native/Studio/Pi acceptance then UI integration.
- #3906a8be72 Learn seam: hosted CI/static/Web/WASM green; Keel current-head
  helper-refactor T2 review still needed. #287 waits actual381 merge.
- #210df81486: required Linux/Windows/sanitizers + private Pi PASS; independent
  T1 inspection no correctness finding. Authored formatting +803cppcheck triage
  remain; do not classify whole static red as inherited. Continue same branch.
- #441ea39c09 OSC inventory selected acceptance/static/Linux/sanitizers PASS;
  correct stale pre-v4 approval request; protected path also named on440.
  #82 is oldQt/CLI, not Electron proof. Keel owner/lifecycle/consent/Monitor
  contract gates runtime. Existing osc.in/monitor IDs; localhost/offLAN.
- #420 SDK64 MERGED84f926b via gate after signed approval; source checks passed.
- Owner enabled #427 auto-merge; newhead464c042 needs integration review/gates.
  Still OPEN, not deployed. Coordinate existing conflict resolver, no duplicate
  branch. After merge verify main deployment/live8patches/upload-update-delete.
- #323385ad20: previous acceptance/required/Web/WASM PASS; current main conflict
  needs normal integration, inherited keyboard-parser classification, new review.
- #31083a4b05 is already based on main: integrate without rewrite; Interleave
  first; current T1 review no High/Medium, hosted PASS; actual3Pi probes pending.
- #398 render evidence MERGED. #4421716e2a: hosted3cases green; Keel safe
  recovery injection ruling before fourth case; #296
  actual3600s fake-device edit-soak missing. #387 reduction waits evidence.
- #4006dcc370 latency: scoped formatting then T2. #43824787c6: Macro baseline
  required Windows/static green; final Keel reset-lifetime disposition remains.
- Chord406/Notes423 identity/channel/off/expression/capacity Keel ruling pending;
  docs-only optional acceptance is not selected evidence. #399 contract gated.
- #456 refreshes STATE/CLAIMABLE/FOUR_AGENTS; fixes455task discovery; latest393
  handoff governs. #439 and Pi installation/setup remain out of scope.

## Pi preparation (owner authorized; installation after Windows test)
- READY #445 preflight/logs: docs/tasks/PI_SOURCE_PREFLIGHT.md; no auto-install.
- READY #446 ARM64 Electron/profile smoke: docs/tasks/PI_APP_SMOKE.md; reuse #386.
- Queue/evidence: #444, docs/tasks/PI_SETUP_QUEUE.md; parent #436, related #235.
- Pi nightly42560014392 pending; actual source SHA/results required, not green.
- #101 baseline pending; #274 display/#296 soak/#271 hardware stay separate.

## Permanent gates
- Required guard/Linux/Windows/sanitizers + task/Studio/WASM/Pi as applicable.
- T1 different-session; T2 cross-family. No IDs/unknown data/threshold changes.
- No force-push/main writes/devices/SSH/LAN; Electron sole automatic restarter.
- No CI polling/schedules. Handoff exact heads, actual evidence and blockers.
