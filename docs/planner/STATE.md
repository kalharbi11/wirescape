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
- MIDI #500: Keel gen-based3s/10s/restart ruling posted; repair builder resumed.
  Forget stale-hint fix/test pushed. Scope1250/16; never guess unknown graph state.
- OSC #72: active A builder task/72-osc-slice-a → B sidecar host/consent → C Studio/Monitor.
  Keel's gen/lifetime/localhost/observation ruling governs; B/C cross-family T2. #94 follows its task; codec-only prep may follow A.
- Groove #505 head592e50d: required/task/Web/Pi pass, Keel approved; auto-merge
  awaits latest ready-event CI. No new source review blocker.
- #509 G2a → #516 G2b retain UI builder; malformed tracks must not recurse/throw.
  Scale persists only in ui.groove[id]; engine config stays version/bpm/tracks.
- Claude UI builder owns both509/516 with normal non-force pushes, then G4b/G5b
  on claude/adoring-shannon-viv6lm; dedicated T1 task527. No duplicate UI writer.
- Native526 has one builder G4a→G5a afterG1; keys rootfirst, no q; div1/2/4/8, cycle512.

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
- #399 head8c340bb: required CI/Pi pass, Keel approved; root normal main sync needed.
  D5 #487 remains distinct from strict-soak513.
- #513/#520 headbbd2e2f: Keel approved; Pi not merge gate per explicit ruling;
  auto-merge awaits fresh ready-event CI. Deadlines/zero-overrun assertions unchanged.
- #522: authorized Studio/native WAV task added; exact acceptance pending.
- #518/#525: slice-A task now authorized; parent72 stays open for B/C.
- #482 R3a head 9d6658d: exact required CI 42757266679 green; Windows 102/102 and
  frozen Qt/current renderer comparison pass. Independent Claude T2 review remains.
- R3b needs mapped replacements and scoped deletion authority; archive/qt-final is
  annotated once on actual merge parent. No deletion/hash rebaseline in R3a.
- #481 R4 follows full R3b; R6 ABI1 #470 stays Keel-led and separate.
- Agent fixes:511 regression builder,512 identity-test helper,500 timeout repair;
  root399/main integration and519/docs; native526 after505. #482 awaits Claude review.
- Permanent: required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
  No force-push, direct main commits, devices/SSH, assertions removed or thresholds weakened.
