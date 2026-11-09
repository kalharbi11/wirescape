<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09 late, Compass + Keel check-in; Codex Lead closeout refresh. Owner failures preempt features.
Live heads and completed evidence govern; queued, stale and missing results never pass.

## Authority and queue
- Board #473 is the claim source. Codex Lead keeps it current and holds the owner's
  delegation to approve/merge after independent review and evidence (T2 still needs Claude).
- Owner's subsequent instruction holds new starts until remaining PRs close, even below10.
- Eight closeout PRs include main compile repair486 and this planner refresh484.
- Later: Learn287 → Groove432 → OSC72; Chord403/421 still waits for S1(b).

## Merged 2026-11-09 (main `605da43`)
- #463 Windows runtime, #400 latency, #442 real-app evidence, #474 PLAN D15–D19.
- Cleanup: R1 #476, R2 #478, R5b #479, R5a docs #477. W1 Windows setup #475.
- Sound/sequencers: #310 Interleave/Vocoder, #405 + #410 16-step, #458 Groove study,
  #451 Groove S1(a) native, #210 MIDI patchable controls (last_note, CC ranges).
- Pi chain: #450 preflight (Pi 28/28 at 1074cfa), #452 ARM64 Electron smoke (Pi pass e6df3d2).
- Compass closed 17 finished issues; finished task files moved to docs/tasks/done/.

## Owner evidence and the Pi session
- Windows first session on #463: WASAPI headphones, starters switch and sound,
  computer keyboard and Launchkey work at 48k/256. Shared WASAPI needs the device rate.
- Pi runner: Node 22.23.4 / npm 10.9.9 and Xvfb/xauth installed by the owner.
- First merge486's reviewed fixture-name repair after exact required/task checks;
  main605da43 currently fails compilation from colliding MIDI/Groove test locals.
- **#483 / #453:** repair the PI_SETUP first-run bypass, merge fixed main into #453, `Closes #483`, `ci/pi` + `ci/pi-accept`. When the
  docs-only exact head passes preflight + app smoke, pin its main parent as `AcceptedSha`
  in docs/PI_FRESH_INSTALL.md (Keel evidence-transfer ruling on #483), then merge.
- Then the owner session: PI_FRESH_INSTALL sections 1–6 + the Pi part of #271.

## Open PRs and rulings
- #399 device routes/rate refusal (T2): **Keel High**: `Pa_IsFormatSupported` runs in
  every warm poll enumeration; on ALSA the active hw device is busy, so the running
  output is marked unsupported. Scoped cooperator repairs probe classification/cache,
  warm polling and three Static findings, with focused tests; Claude re-review remains.
- #461 Groove saturation/replan tests (T0): root integrates fixed main, retains reviewed
  two-file delta; final required/task/Pi disposition follows.
- #480 refused draft: f72ce9f fixes the last Save-during-load Medium with delayed
  bridge/Electron file tests; independent source review passed, hosted gates pending.
- #482 R3a:73ad9b1 splits nondeleting acceptance per Keel. Actual frozen-Qt comparison
  remains required: repeatability alone is not cross-renderer equality. No hash
  rebaseline. R3b deletion and allow-test-removal require a
  test-to-replacement table; Codex Lead tags `archive/qt-final` (annotated) on
  R3b's merge parent, once.
- #481 R4 follows R3. R6 ABI1 #470 is Keel's, after R3.

## Housekeeping notes
- AGENTS.md/WORKFLOW.md still say "until gate v4 merges"; gate v4 merged (#407).
  Fix with the next allow-protected rules PR (W_GATE_V4.md stays until then).
- docs/tasks/README.md lists task files still cited by open work; move them to done/
  when those PRs merge.
- Permanent: required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
  No force-push, direct main commits, device/SSH access, suppression or threshold weakening.
- #271 owner hardware, #274 display/performance and #296 actual 3600 s soak stay separate.
