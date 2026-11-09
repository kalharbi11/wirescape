<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09, Codex Lead. Owner failures preempt features.
Live heads and completed evidence govern; queued, stale and missing results never pass.

## Authority and queue
- Board #473 is the claim source. D18 holds while 10+ implementation PRs are open.
- Owner explicitly delegated Codex Lead authority to revise rulings, unblock agents,
  approve and merge; Keel availability alone does not block. Record decisions on issues.
- Keep independent review, actual evidence, realtime safety, IDs and unknown data.
- PLAN D15–D19 and CLEANUP govern scope. No new module/feature issues in this window.

## Merged / current lead gates
- #474 R0 merged83b381a; #442 real-app evidence merged32e7ba4.
- #400 latency reporting merged9a1d2f5 after Keel review, exact-head required/task
  green and pinned LLVM18 formatting proof; no Pi requirement for reporting-only scope.
- #463 Windows runtime merged3fa0d3a after cross-family review and integration gates.
  Pi build/accept passed31b909a; the later CLI whitespace wrap was verified token/tree
  equivalent. The lead recorded that bounded evidence transfer, not a new Pi run.
- #399/#472/#471 must merge the actual #463 main before their final review.

## Owner evidence and setup
- Windows first session on #463: WASAPI headphones, all starters switch and sound,
  computer keyboard and Launchkey MIDI work at48k/256. Extended testing remains.
- Shared WASAPI needs the device rate and another DAW must release the device.
- Pi runnerpi5 is online and executing private poll42565126954; no duplicate dispatch.
- Pi chain #450 74c5ee2 -> #452 98755f4 -> #453 558e52d: main integrated;
  exact-head Pi evidence and the common accepted SHA remain pending.
- Owner confirmed Xvfb/xauth installed; #452 has ci/pi-retry after that fix.
  One accepted SHA must contain #463/#450/#452 and pass combined device-off ARM64 smoke.
- #475 W1 e8e9779: earlier CMake identity failure fixed; preserve confinement/DLL
  validation. Executed real Windows device-off setup smoke remains a merge gate.

## Active cleanup and rulings
- #464/#476 R1 merged ab64493: Qt-free default and shared PortAudio header/license
  exact renames. The old ABI1 and Qt-removal packets remain separate.
- #466/#477 R5a docs: wording reviewed; main/STATE refresh awaits final hosted gates.
- #465/#478 R2 1a07a5d: source review approved the ingress guard and Electron
  overwrite/v1-backup protections; exact-head hosted/task gates remain pending.
- #467/#479 archives100 verified closed tasks with identical hashes; final integration
  gates pending. PLANNER_HANDOFF deletion is deferred to R5c to preserve AGENTS links.
- Qt-only inspector behavior is explicitly retired; supported compatibility and real
  persistence guarantees must be mapped in R2 table before R3 deletes tests.
- R3 #468 waits R1/R2/W1 and archive/qt-final on its base; R4 #469 follows R3;
  R6 #470 remains separate ABI1 retirement. No Qt tests or ABI1 deleted yet.

## Retained builders / permanent gates
- #405 -> #410; #451/#461 -> playhead -> editor; #210 MIDI; #310 Interleave;
  #458 study reconnect/task fixes. Existing builders retain branches; see board.
- Required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
- No force-push, direct main commits, device/SSH access, suppression or threshold weakening.
- #271 owner hardware, #274 display/performance and #296 actual3600s soak remain separate.
