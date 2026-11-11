<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-11, Keel (finish-first rulings). Owner failures preempt features.
Live heads and completed evidence govern; queued, stale and missing results never pass.

## Authority and queue
- Board #473 and newest SEND THESE comment are the live claim source; retain fresh claims.
- Owner/Keel ASAP lanes: Chord, MIDI Learn, OSC and Groove. Other new features wait.
- Codex Lead/Coda share senior routing; T2 requires the other model family.
  Keel-only ABI/SDK/spec/CI-policy authority remains unchanged.
- Chord #403/#511 and Notes→CV #421/#512 may merge independently; preserve registrations.
  Explicit note identity reuse is last-wins; anonymous Notes→CV releases are FIFO.
- #517 reconciles Notes→CV to FIFO and <=18 files/900 lines, retaining tests.
- #511 at1c96ceb: task/Linux/sanitizers pass; inherited Groove emit-guard Static failure
  and Windows seed-278 wake overrun remain. #552/#553 supplies strict guard repair.
  Integrate after merge; Keel classifies scheduling recurrence under #513, no weaker limit.
- #512 at7f16d17: reported exact hosted green, but branch conflicts. Existing writer
  integrates main; independent changed-head T1 and actual Pi module CTest/ruling remain.
- Native Groove #526/#528 atcdc474f: non-draft and mergeable, Claude T2 COMMENT and
  actual Pi Groove CTests present. Exact-head workflow lookup empty: verify hosted gates.
- Groove UI #527/#534 at3ca6113: draft/conflicted on merged #516 parent.
  Claude retains claude/adoring-shannon-viv6lm; normally integrate/retarget to main.
  Fix untouched-config tests, existing-key double-click deletion and held-chord cleanup
  on blur/hidden/focusout with regression. Preserve native #528 dependency.
  Scale stays in ui.groove[id]; native keys root-first, div1/2/4/8, cycle512.
- OSC #72: A/#518 merged. B/#529 already claimed on task/529-osc-host; resume host
  lifecycle/consent outside specs. Keel must authorize ENGINE_PROTOCOL.md on #529.
  C follows B; cross-family T2. #94 follows complete J5 per its task, not A alone.
- #551 Chromium acceptance fix: narrow T0 source reviewed; conflicting, missing
  Windows/Web evidence. Existing writer integrates main and reruns full acceptance.
- #553 atef3294b: strict emit guard source reviewed, no findings; required/Static
  checks incomplete at review, Windows absent. No approval until final evidence.

## 2026-11-11 Keel (owner: finish MIDI/OSC/Groove/CHAOS + existing-module fixes, then move on)
- Merged since: Chord #511, Groove native #528 + UI #534 (closes #514/#523/#526/#527), Windows wake #572,
  patch offline smoke #568, Qt probe retirement #567.
- #554/#565 CHAOS performer strip dropped (not planned); branch kept as archive, tag pending (Codex Lead).
- Rulings: #528 G1 IDs byte-identical, chord IDs untagged but unique; #529 ENGINE_PROTOCOL authorized
  (hello `osc`, `status.osc`, `--confirm-lan`); #337 nominal-pitch readout; #564 docs-only evidence transfer.
- #569 CI speed: PR-only cancel for sanitizers/acceptance + ccache sanitizers (Codex T2).
- Qt tests left are R3 only; Qt-only `allow-test-removal` pre-authorized once R2 replaces golden/app_patch coverage.

## Completed baseline and owner evidence
- Newly verified merged: #399 device routes, #500 MIDI Learn, #505 G1, #509 G2a,
  #516 G2b, #518 OSC A, #520 scheduler, #533 CHAOS LFO and #546 curated layout.
- Earlier merged: #486, #484, #461, #480, #494, #492, #453, #501, #504, #506–#508.
- #453 pins AcceptedSha cd7999e4a02308df0db918cfbab6974b97b6782f under Keel's
  docs-only transfer: private pi5/accept42720681421, preflight28/28 and app smoke pass.
  #483 closed; later main is not certified by that evidence.
- Owner next: PI_FRESH_INSTALL sections1–6 and #271 Pi portion. Windows first session
  verified WASAPI headphones, keyboard and Launchkey at48k/256.
- #271 hardware, #274 display/performance, #296 actual3600-second soak remain distinct.

## Closeout and evidence limits
- #482 R3a: frozen Qt/current renderer equality and independent review; no deletion
  or hash rebaseline. R3b needs replacement map/scoped authority and archive/qt-final
  annotated once on actual merge parent. #481 R4 follows full R3b; #470 R6 is Keel-led.
- #543/#544/#547/#548/#549 retain existing writers; close acceptance/review gates.
- Local checkout29611d2: acceptance-tool tests2/2, doc links2/2, all15 shared patches
  structurally pass. Doctor stops: CMake absent; no native/newer-head acceptance claimed.
- #519 carries this state refresh; normal main integration and exact-head checks remain.
- Permanent: guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
  No direct main commits, force pushes, devices/SSH or weakened assertions/thresholds.
