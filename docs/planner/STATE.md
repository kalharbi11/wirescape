<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 by Codex Lead (Codex). **Pi first; hardware checks remain deferred by owner.**

## This session
- Read AGENTS, WORKFLOW, LEADS, current plan and live issue/PR board.
- **#306 → draft #311**, head `eda17d4`: generic conditional parameter controls.
  Saved base values govern visibility; hidden controls retain values and cables.
  Empty sections hide; geometry/anchors update when visibility changes.
  Catalogue pass-through tests (4), catalogue drift and whitespace checks pass.
  Full hosted Web at 95f13c0 passed (42364943723), ARM64/sanitizers passed.
  eda17d4 routes mandatory UI acceptance to provisioned Web (native job lacks npm).
  Local Python/catalogue/size pass. New-head Web + independent T1 review pending.
  Local npm and GitHub CLI still blocked by proxy HTTP 503.
- **#300 (#289)** retargeted from #283 to main, integrated with main at `e4e0ae8`.
  No force push. Current main's on-screen keyboard fixes preserved.
  Only the prior reviewed six-file preference delta remains against main.
  Independent review 5806280884; integrated-head CI 42364843963 and Web 42364844011 green.
  Marked non-draft/ready-to-merge; signed gate approval posted for e4e0ae8.
  Non-required portability red remains #159; merge not claimed.

## Current queue (board snapshot; not a claim of green CI)
- Phase 1: #256/#250 protocol conformance; #274/#258 benchmark; #235 bring-up.
- Phase 2: #273/#267 live sidecar edits; #298/#288 MIDI hotplug;
  #301/#291 audio recovery. These T2 lanes still need cross-family review/evidence.
- #299/#297 remains a blocked scheduling diagnostic, not an approved fix.
- #283 screen keyboard and #284 edit-soak harness are merged in this checkout.
- Unclaimed Phase 2 issues: #287 MIDI Learn, #292 load/dropouts, #293 device details
  (after #291), #294 patch browser, #295 recovery, #296 private-runner soak.
  Recheck latest claims before starting; previous lead state was stale.
- #210/#290 patchable MIDI controls waits on its recorded contract ruling.
- Sound kit: #304 Spectral A and stacked #310/#305 Spectral B open.
  #306 generic visibility is independent. #307 latency explicitly waits on Keel.
- #309/#308 browser demo open; implementation/review is a separate lane.
- Windows #266 remains deferred behind Pi priority. Qt remains frozen.

## Next independent work
- Read new-head #311 evidence once available and obtain independent T1 review.
- #300 has gate approval; verify actual merge next session.
- Pick #294 or #295 for isolated UI work once dependencies are available.
- Leave queued hosted/Pi CI to run; no polling, SSH, real devices or LAN tests.

## Automatic continuation
- Owner explicitly requested recurring engineering runs: every 150 minutes through
  Monday 2026-11-09 06:00 UTC. Enabled; first scheduled 2026-11-07 13:31 UTC.
- Advance work each run, skip blocked lanes, avoid duplicate claims/overlapping edits.

## Owner
- Nothing needed now. #271 hardware session and checkpoints remain outstanding,
  intentionally skipped under this run's instruction.
Signed: Codex Lead (Codex), 2026-11-07
