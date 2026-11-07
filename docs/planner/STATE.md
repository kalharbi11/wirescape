<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 by Codex Lead (Codex). **Pi first; hardware checks remain deferred by owner.**

## This session
- Read AGENTS, WORKFLOW, LEADS, current plan and live issue/PR board.
- **#306 → draft #311**, head `95f13c0`: generic conditional parameter controls.
  Saved base values govern visibility; hidden controls retain values and cables.
  Empty sections hide; geometry/anchors update when visibility changes.
  Catalogue pass-through tests (4), catalogue drift and whitespace checks pass.
  Acceptance size passes (137 lines / 7 files). Typecheck/Playwright remain unverified:
  npm registry and GitHub CLI requests return proxy HTTP 503; dependencies unavailable.
  Task: `docs/tasks/W_VISIBLE_WHEN.md`. Needs hosted Web + independent T1 review.
- **#300 (#289)** retargeted from #283 to main, integrated with main at `e4e0ae8`.
  No force push. Current main's on-screen keyboard fixes preserved.
  Only the prior reviewed six-file preference delta remains against main.
  Prior review: 5806280884; historical browser passes do not validate this new head.
  Remains draft until fresh hosted Web/Electron acceptance and integration review.

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
- Restore dependency access, then run #311 typecheck/browser acceptance and T1 review.
- Validate #300's integrated head; do not reuse old-head CI as evidence.
- Pick #294 or #295 for isolated UI work once dependencies are available.
- Leave queued hosted/Pi CI to run; no polling, SSH, real devices or LAN tests.

## Owner
- Nothing needed now. #271 hardware session and checkpoints remain outstanding,
  intentionally skipped under this run's instruction.
Signed: Codex Lead (Codex), 2026-11-07
