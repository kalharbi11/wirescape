<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-09, Codex Lead closeout round.
Live heads and completed evidence govern; queued, stale and missing results never pass.

## Authority and queue
- Board #473 is the live claim source. Owner requests clearing the remaining PRs
  before new features and the next cleanup/organizing window, even below10 open.
- Owner delegated Codex Lead authority to revise rulings, unblock, approve and merge;
  keep independent review, actual evidence, realtime safety, IDs and unknown data.
- PLAN D15–D19 and CLEANUP govern scope. Existing builders retain their branches.

## Merged / evidence
- Existing foundation: #463 Windows runtime3fa0d3a; #475 W1 setup611286f;
  #476 R1ab64493; #478 R2 coverage16e49e9; #479 archive resolver118e319.
- This round merged #410 SEQ16 editor5a6dcdd; #477 source quickstartsce656b2;
  #450 Pi preflightfcf38b9; #452 Pi app smoke20aabc0;
  #451 Groove S140b3b51; #210 MIDI controls605da43.
- All six had exact-head guard/Linux/Windows/sanitizers/task success and independent
  review. Applicable Web/WASM results passed; no new hardware result is claimed.
- Actual Pi acceptance passed4501074cfa (28 preflight predicates),
  452e6df3d2 (ARM64 Electron+native device-off smoke),451847d624 (Groove),
  4613b2356f (stacked host-saturation proof),210a7b3835 (60s synthetic MIDI,
  2,880,000frames,2816paired notes/releases,zero allocations/faults).
- Groove Static red is cppcheck's intentional macro-restoration #error branch;
  MIDI red is inherited Qt emit-model parsing at existing b.emit expressions.
  Authored clang-tidy/format checks pass. No assertions or scanners weakened.

## Six remaining PRs / next actions
- #461: parent451 merged; retargeted main. Existing builder integrates current main
  normally, preserves the reviewed two-file test delta and reruns final checks.
- #399:08a8bce required/task/Web/WASM and actual Pi5/5 pass. Three authored Static
  repairs remain: empty catch, unnecessary test copy, enumeration reserve.
  Existing cleanup claimant finishes the narrow repair, then changed-head review.
- #480:484961e hosted checks pass, but independent review5828294870 leaves one
  Medium: native Save must block during every documentBusy ordinary load.
  Add delayed-load/menu-Save regression; keep draft until repair/review/checks.
- #453: integrate merged450/452 and route the combined source through the existing
  private bridge for actual preflight+ARM64 app smoke, then pin that immutable SHA.
  Main605da4368d469645810adbdc9f70d70789741079 contains all prerequisites;
  no combined accepted SHA exists yet. Separate branch passes are not combined proof.
- #482 / R3: R1/R2/W1 merged. Finish native/frozen golden comparison before deletion;
  preserve audio/event hash bytes, finite/fault/equality/sensitivity assertions.
  Exact deletion/replacement mapping, scoped labels, final main integration,
  independent review and archive/qt-final on the last Qt-containing base remain.
- #481 / R4:1490535 independently reviewed; local/required results retained.
  Wait R3+archive tag, integrate main normally, run final acceptance and hosted gates.
- R6 #470 remains separate ABI1 retirement after R3. No ABI1 removal this round.

## Owner evidence / permanent gates
- Owner Windows first session: WASAPI headphones48k/256, starters, keyboard/Launchkey.
  Node22.23.4/npm10.9.9 and Xvfb/xauth installation were owner-confirmed.
- Required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
- No force-push, direct main commit, agent SSH/device access or threshold weakening.
- #271 owner hardware, #274 display/performance and #296 actual3600s soak stay separate.
