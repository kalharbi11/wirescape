<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-10, Codex Lead round. Owner failures preempt features.
Exact-head completed evidence governs; pending, historical and missing results never pass.

## Authority and queue
- Board #473 and its SEND THESE comment are the live claim source; retain fresh claims.
- Owner/Keel lifted the hold for Chord, MIDI Learn, OSC and Groove; other new starts wait.
- Codex Lead/Coda share senior routing/fix authority per owner. T2 still requires the other
  model family; Keel-only ABI/SDK/spec/CI-policy authority is unchanged.
- Chord #403/#511 and Notes→CV #421/#512 may merge independently. Preserve both registrations.
  Keel: explicit identity reuse is last-wins. Coda: Notes→CV anonymous offs are FIFO.
- #517 reconciles Notes→CV task to FIFO and ≤18 files; ≤900 lines/tests unchanged.
- MIDI Learn #287/#500: active builder serializes edits during one atomic loadGraph.
  Visible pending state; ack/refusal/timeout and rollback regressions before T1 re-review.
- OSC #72: active A builder task/72-osc-slice-a → B sidecar host/consent → C Studio/Monitor.
  Keel's gen/lifetime/localhost/observation ruling governs; B/C cross-family T2. #94 after A.
- Groove: G3 #501 merged; #505 G1 must merge main normally, repair Static and obtain
  fresh required/task/Web/Pi plus Claude T2 re-review. #509 G2a → #516 G2b retain stack.
- #514 G4a starts after #505 (not Chord); G4b after G2 PatchDoc. Pads root + "+N" only.

## Completed baseline and owner evidence
- Merged: #486 compile repair, #484 planner, #461 saturation, #480 refused draft,
  #494 Pi smoke, #492 Macro names, #453 Pi guide, #501 G3, #504 setup,
  #506 workflow, #507 patch library and #508 module tasks.
- Earlier Phase 1/native/Windows/cleanup evidence remains in PLAN and merged handoffs.
- #453 pins AcceptedSha cd7999e4a02308df0db918cfbab6974b97b6782f under Keel's
  docs-only transfer ruling: private pi5/accept 42720681421, preflight 28/28 and
  ARM64 Electron/native smoke passed. #483 closed; later main is not certified by it.
- Owner next: PI_FRESH_INSTALL sections 1–6 + #271 Pi portion. Windows first session
  already verified WASAPI headphones, keyboard and Launchkey at 48k/256.
- #271 hardware, #274 display/performance and #296 actual 3600-second soak stay separate.

## Closeout and review gates
- #399 head 8c340bb: Keel delegated probe/cache High re-review to Claude Coda.
  Pi passed; one authorized Windows rerun dispatched. Prior failure is D5 long-block
  swap timeout (#487), distinct from #513 strict-soak overruns. No repeated retries.
- #513 builder: Keel-approved Windows null-sink MMCSS/timer lifetime and bounded
  diagnostics. Absolute deadlines/zero-overrun assertions stay intact. Claude T2 review.
- #482 R3a head 9d6658d: exact required CI 42757266679 green; Windows 102/102 and
  frozen Qt/current renderer comparison pass. Independent Claude T2 review remains.
- R3b needs mapped replacements and scoped deletion authority; archive/qt-final is
  annotated once on actual merge parent. No deletion/hash rebaseline in R3a.
- #481 R4 follows full R3b; R6 ABI1 #470 stays Keel-led and separate.
- Coda T2 batch posted on #473: #482, #399, then changed #505 and future #513.
- Permanent: required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
  No force-push, direct main commits, devices/SSH, assertions removed or thresholds weakened.
