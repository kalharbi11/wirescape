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
- W1 #475 merged 611286f; offline Groove study #458 merged f7c239e.
- Spectral Interleave/Vocoder #310 merged dbe680a; SEQ-16 native #405 merged e1618f1.
- Live claims/heads and remaining integration work are tracked on board #473.

## Owner evidence and setup
- Windows first session on #463: WASAPI headphones, all starters switch and sound,
  computer keyboard and Launchkey MIDI work at48k/256. Extended testing remains.
- Shared WASAPI needs the device rate and another DAW must release the device.
- Pi runner is online. Owner confirmed Node v22.23.4 / npm 10.9.9 and Xvfb/xauth
  installed; current #450 1074cfa / #452 e6df3d2 evidence remains pending.
- Drive #450 -> #452 -> #453 to one accepted SHA; exact-head Pi and combined-main
  device-off ARM64 app smoke remain gates. No common accepted SHA is recorded.
- Merged W1 includes real Windows device-off smoke evidence; combined-main checks
  and the accepted owner install SHA remain pending. See docs/WINDOWS_SETUP.md.

## Active cleanup and rulings
- #464/#476 R1 merged ab64493: Qt-free default and shared PortAudio header/license
  exact renames. The old ABI1 and Qt-removal packets remain separate.
- #466/#477 R5a docs: wording reviewed; main/STATE refresh awaits final hosted gates.
- #465/#478 R2 1a07a5d: source review approved the ingress guard and Electron
  overwrite/v1-backup protections; exact-head hosted/task gates remain pending.
- #467/#479 merged 118e319: 100 verified closed tasks archived with identical hashes.
  PLANNER_HANDOFF deletion remains deferred to R5c to preserve AGENTS links.
- Qt-only inspector behavior is explicitly retired; supported compatibility and real
  persistence guarantees must be mapped in R2 table before R3 deletes tests.
- R3 #468 waits R2 and current-main/qt-final on its base; R4 #469 follows R3;
  R6 #470 remains separate ABI1 retirement. No Qt tests or ABI1 deleted yet.

## Retained builders / permanent gates
- #410 follows merged #405; #451/#461 -> playhead -> editor; #210 MIDI continues.
  Existing builders retain branches; board #473 is the live claim/head source.
- Required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
- No force-push, direct main commits, device/SSH access, suppression or threshold weakening.
- #271 owner hardware, #274 display/performance and #296 actual3600s soak remain separate.
