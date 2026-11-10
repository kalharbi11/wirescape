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
- MIDI #500 head8b909a5: Forget stale-hint fix/test pushed; unanswered-load High awaits
  Keel fail-closed recovery ruling. Scope1250/16; never assume timeout means rejection.
- OSC #72: active A builder task/72-osc-slice-a → B sidecar host/consent → C Studio/Monitor.
  Keel's gen/lifetime/localhost/observation ruling governs; B/C cross-family T2. #94 follows its task; codec-only prep may follow A.
- Groove: #505 integrated main7cb25d9 at592e50d; observation test passes locally;
  exact required/task/Web/Pi and Claude T2 re-review remain.
- #509 G2a → #516 G2b retain UI builder; malformed tracks must not recurse/throw.
  Scale persists only in ui.groove[id]; engine config stays version/bpm/tracks.
- #521 design/#524 rulings merged: #514 G4 chords → #523 G5 speed, one native
  builder after G1; one UI builder after G2. No stored q; div1/2/4/8, max cycle512.

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
- #399 head8c340bb: authorized Windows rerun/all required CI pass; Pi passed.
  Claude probe/cache High re-review remains. D5 #487 is distinct from strict-soak513.
- #513/#520 headbbd2e2f: required Windows/Linux/sanitizers/task green. Actual private
  Pi and Claude T2 review remain; absolute deadlines/zero-overrun assertions unchanged.
- #522: authorized Studio/native WAV task added; exact acceptance pending.
- #518/#525: slice-A task now authorized; parent72 stays open for B/C.
- #482 R3a head 9d6658d: exact required CI 42757266679 green; Windows 102/102 and
  frozen Qt/current renderer comparison pass. Independent Claude T2 review remains.
- R3b needs mapped replacements and scoped deletion authority; archive/qt-final is
  annotated once on actual merge parent. No deletion/hash rebaseline in R3a.
- #481 R4 follows full R3b; R6 ABI1 #470 stays Keel-led and separate.
- Coda T2 batch posted on #473: #482, #399, then changed #505 and future #513.
- Permanent: required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
  No force-push, direct main commits, devices/SSH, assertions removed or thresholds weakened.
