<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09 late, Compass + Keel (Claude) check-in. Owner failures preempt features.
Live heads and completed evidence govern; queued, stale and missing results never pass.

## Authority and queue
- Board #473 is the claim source. Codex Lead keeps it current and holds the owner's
  delegation to approve/merge after independent review and evidence (T2 still needs Claude).
- **D18 is lifted:** 6 PRs open. It re-arms automatically at 10+ implementation PRs.
- Next starts, one lane each, in order: MIDI Learn UI #287 → Groove S1(b) playhead +
  stopped editor (#432) → OSC slice A #72 (T2). Chord #403/#421 waits for S1(b).

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
- **#483 / #453:** merge main into #453, `Closes #483`, `ci/pi` + `ci/pi-accept`. When the
  docs-only exact head passes preflight + app smoke, pin its main parent as `AcceptedSha`
  in docs/PI_FRESH_INSTALL.md (Keel evidence-transfer ruling on #483), then merge.
- Then the owner session: PI_FRESH_INSTALL sections 1–6 + the Pi part of #271.

## Open PRs and rulings
- #399 device routes/rate refusal (T2): **Keel High**: `Pa_IsFormatSupported` runs in
  every warm poll enumeration; on ALSA the active hw device is busy, so the running
  output is marked unsupported. Fix: classify probe errors (only paInvalidSampleRate
  = unsupported), no probing in warm polls, never re-probe the active device. Blocked.
- #461 Groove saturation/replan tests (T0): existing builder; merge on green + Pi.
- #480 refused-patch draft (#471): three review Mediums; Codex Lead repairing.
- #482 R3: Keel rulings on #468: golden port approved in principle (no hash
  rebaseline), split R3a goldens / R3b deletion; allow-test-removal after a
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
