<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09, Keel open-PR closeout. Owner-reported failures preempt everything.
No queued, stale or missing result counts as a pass. Live heads and handoffs govern.

## Now: finish before start (D18, Compass #437)
- Canonical queue: `docs/tasks/PR_CLOSEOUT_2026_11_09.md` (lanes A–J). It replaces
  the READY list in `CLAIMABLE_2026_11_09.md`. No new module/feature issues until the
  open count is under 10. Compass's D15–D19 + `docs/planner/CLEANUP.md` are on branch
  `claude/hopeful-hopper-wh0id2` (#437). Cleanup R1–R7 starts after the closeout.
- Open: 14 PRs (down from 22). Merge order: #427, #323, #400, #442, #310, #210,
  #405 → #410, #451 → playhead PR, then #399, #458, the Pi PRs and #453.

## Keel today (2026-11-09)
- Merged #456, #438 (merge commit), #441, #390 (T2 final at 6a8be72; #287 unblocked).
- Closed #82 (Qt OSC; port from branch), #443 (empty), #406 and #423 (parked per D18).
- #427: main merged → `486d6b9`, approval posted; the gate merges on green.
- #323: helper merging main (catalogue union); T1 review stands; static is inherited.
- #400 T2: no High/Medium; conditional approval after a formatting-only head.
- Rulings: #432 Groove `t1_pos`/`t2_pos` + `playhead` telemetry, capability
  `telemetry.playhead`; #386 case 4 = native fake-service CTest + Studio mock-bridge
  state sequence (no test hooks); #293 optional device fields, mono disabled,
  rate/block stream-only restart with rollback, id-only identity; #72 OSC host in the
  sidecar Session, `--confirm-lan`, `status.osc`, `monitor` telemetry (slice A is T1).
- Static rule: fix authored formatting on changed hunks; cppcheck on untouched lines
  is inherited and Codex Lead merges past it (delegation c). No new suppressions.

## Keel owes next session
- T2 cross-family reviews when ready: Groove playhead PR, #399/#293 implementation,
  OSC slices B/C, #296 private soak review. Chord/Notes→CV ruling when Chord is next.
- R2 (document coverage swap) review and R6 (ABI1 retirement) lead, after the closeout.
- Low: the `module2.h` `latency_frames` comment should state the threading rule (label-only).

## Pi preparation (after the Windows test)
- #450: fix the cache false positive and read-only `--check-only`. Owner installs Node 22.
- #452: short-header tests; `xvfb-run` smoke, else reported blocked. Owner installs xvfb.
- #453 guide updates to one accepted SHA after both merge (renamed PI_SETUP in R5).
- #101 baseline; #274 display, #296 soak and #271 hardware stay separate.

## Sequencers (one at a time)
- 16-step #405 → #410, then Groove S1 #451 → playhead PR → S1(b) editor, then Chord.

## Pi preparation (owner authorized; installation after Windows test)
- READY #445 preflight/logs: docs/tasks/PI_SOURCE_PREFLIGHT.md; no auto-install.
- READY #446 ARM64 Electron/profile smoke: docs/tasks/PI_APP_SMOKE.md; reuse #386.
- Queue/evidence: #444, docs/tasks/PI_SETUP_QUEUE.md; parent #436, related #235.
- Pi nightly42560014392 pending; actual source SHA/results required, not green.
- #101 baseline pending; #274 display/#296 soak/#271 hardware stay separate.

## Permanent gates
- Required guard/Linux/Windows/sanitizers + task/Studio/WASM/Pi as applicable.
- T1 different-session; T2 cross-family. No ID, unknown-data or threshold changes.
- No force-push, main writes, devices/SSH/LAN; Electron is the sole automatic restarter.
- No CI polling/schedules. Handoffs give exact heads, actual evidence and blockers.
