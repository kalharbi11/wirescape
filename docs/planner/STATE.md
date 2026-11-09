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
- #463 Windows runtime: cross-family source review complete. All hosted checks passed
  a6168ed. Actual Pi build/accept passed31b909a; only a CLI whitespace wrap differed,
  verified token/tree equivalent. Lead recorded that bounded evidence transfer.
- #463 now integrates current main normally; only STATE conflicted, native merged cleanly.
  Fresh integration checks/evidence remain before actual merge. Then #399/#472/#471
  owners merge main. No claim that an untested integration head passed on Pi.

## Owner evidence and setup
- Windows first session on #463: WASAPI headphones, all starters switch and sound,
  computer keyboard and Launchkey MIDI work at48k/256. Extended testing remains.
- Shared WASAPI needs the device rate and another DAW must release the device.
- Pi runnerpi5 is online and executing private poll42565126954; no duplicate dispatch.
- Pi chain #45052ade08 -> #452926a5e5 -> #453d0c45ca: source reviewed,
  repaired Windows/static checks, actual final target evidence/combined SHA pending.
- Owner installs Node22/xvfb where required. One accepted SHA must contain actual
  #463/#450/#452 merges and pass combined device-off ARM64 app smoke.
- #475 W1 Windows setup: existing builder plus delegated repair reviewer; fix generated
  output junction escapes, DLL load/export validation and executed Windows smoke.

## Active cleanup and rulings
- #464/#476 R1 Qt-free build: source reviewed; header/license exact renames.
  Size2800/14 accounts for2618 unchanged moved lines, not a test threshold change.
- #466/#477 R5a docs: independent wording review complete, hosted gates pending.
- #465 R2: real unsupported/future Studio ingress data-loss guard and Electron
  overwrite/v1-backup protections; independent review/full tests required.
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
