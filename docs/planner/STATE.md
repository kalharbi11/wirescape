<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09, Codex Lead merge-captain handoff. Owner-reported failures preempt everything.
No queued, stale or missing result counts as a pass. Live heads and handoffs govern.

## Now
- **Claimable work lives on GitHub: board issue #473.** Free agents take the first
  unclaimed ready row and claim on that row's issue. Codex Lead keeps the board ticked.
  Per-PR rulings: `docs/tasks/PR_CLOSEOUT_2026_11_09.md`. Plan: D15–D19 in `docs/PLAN.md`,
  sequence in `docs/planner/CLEANUP.md` (R0 #474; R1 #464, R2 #465, R5 #466/#467 open now;
  R3 #468, R4 #469, R6 #470 blocked).
- **Windows:** owner's first install and test passed on the #463 branch (real audio,
  keyboard, Launchkey, all starters switch). #463 cross-family source review is complete
  at 31b909a (final explicit-flush fix recheck pending). Prior a30 required/task/Web
  checks passed; final CI/Pi remain gates. Anything touching
  `native/sidecar` or `native/io/audio` (#399, #472) merges main after it.
  #436 W1 = `docs/WINDOWS_SETUP.md` + `tools/windows/setup.ps1` at #463's merge SHA.
- **Pi tomorrow (owner):** #450 → #452 → #453 with one accepted SHA. Owner installs
  Node 22 and `xvfb` on the Pi first; runner is rebooting and actual Pi jobs remain queued.
- Open PRs: 14 (D18 still in force: no new module/feature issues until under 10).

## Keel today (2026-11-09)
- Merged #456, #438, #441, #390, #459, #323 (Quantizer, 3ed2a14 after catalogue union),
  and #427 (gate). Compass's R0 decisions remain in #474 until it merges.
- #442 merged 32e7ba4: independent T1, exact-head required/task/Web checks passed.
- Closed #82, #443, #406, #423 (parked), #188 (Qt launcher, superseded by #436).
- Rulings: #432 playhead seam, #386 case 4, #293 devices, #72 OSC, #400 conditional
  approval, #472 rate-mismatch refusal (`rate-unsupported`, never `reconnecting`).
- Static rule: fix authored formatting on changed hunks; cppcheck on untouched lines is
  inherited and Codex Lead merges past it (delegation c). No new suppressions.

## Keel owes next session
- T2 cross-family reviews: Groove playhead PR, #399 + #472, R2 #465 coverage table,
  R3 #468, OSC slices B/C, #296 soak. Lead R6 #470 after R3. Set the `archive/qt-final`
  tag on R3's base commit before it merges.
- Low: `module2.h` `latency_frames` comment states the threading rule (label-only).

## Sequencers (one at a time)
- 16-step #405 → #410, then Groove S1 #451/#461 → playhead PR → S1(b) editor, then Chord.

## Permanent gates
- Required guard/Linux/Windows/sanitizers + task/Studio/WASM/Pi as applicable.
- T1 different-session; T2 cross-family. No ID, unknown-data or threshold changes.
- No force-push, main writes, devices/SSH/LAN; Electron is the sole automatic restarter.
- No CI polling/schedules. Handoffs give exact heads, actual evidence and blockers.
