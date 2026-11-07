<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 16:30 UTC by Compass (platform check-in). **Pi first. Phase 2 runs beside Test Session 1. The web demo (D14) is a separate lane. Any owner failure in #271 preempts all work.**

## Lead split (owner, 2026-11-07)
- **Compass (platform):** everything except modules: Phases 1–5, the app, the engine lanes, the web demo, the board and STATE. Checks in every 4 h (owner-requested scheduled run).
- **Compass (modules):** Track S and the module lane (#304/#310, #314–#318, #305/#306/#307). Leaves this file's module lines to that session.
- Codex Lead runs every 150 min through 2026-11-09 06:00 UTC (owner-requested).

## Compass actions (16:12 check-in)
- #320 (#297 fix): cross-family review of the GPT follow-up at `360ea3b` found nothing, so I approved it to auto-merge on green. Strict single-attempt mode keeps the pi5 `--max-gap-ms 20` evidence meaning what it did.
- #256 (1H): merged main in (`01305a3`) and ruled that Keel's no-Pi review covers its `pi:` line. Approved for auto-merge on green; it closes 1H.
- #321 (W-web-2..5): merged main in (`f8f203f`), two T1 reviews already done. Approved for auto-merge.
- #325 (Codex Lead STATE): approved. This file is built on top of it.
- Asked Keel (on #273) for the four stalled T2 reviews, in this order: #274 → #273 → #301 → #298.

## Main blocker
- **T2 review throughput.** #273, #274, #298 and #301 have been waiting on Keel since 11:11 UTC, and they gate the rest of Phase 2. #297 is resolved by #320.

## In flight (who acts)
- #273 live replan in sidecar `00a5b24`, #274 bench summary `c6ed064`, #298 MIDI hotplug, #301 audio-loss recovery: Keel T2 review plus pi5, then merge. #274 unblocks #292/#296; #273 unblocks #287; #301 unblocks #293.
- #256 1H conformance: approved at `01305a3`, auto-merges on green. If web goes red on the merged head, fix it with a small change; don't re-scope.
- #300 and #311 merged.
- #210 / #290 MIDI controls: Keel/Compass contract ruling pending.
- #295 crash recovery: draft #324 at 9b3e697; added expected-exit and pending-offer reload regressions (11 total).
  be1bebc had green hosted CI/Web and independent review (no implementation defects); two Low tests addressed.
  New-head hosted Web/Electron evidence pending; local npm/git remain blocked by proxy HTTP503.
- **Free now, unclaimed for 5 h (Codex Lead to queue first):** #294 patch browser.
- Still blocked: #287 (after #273), #292 and #296 (after #274), #293 (after #301).

## Web demo lane (D14)
- #309 merged. #321 (W-web-2..5: starters, telemetry, share link, save/open, live-site check) is approved at `f8f203f`. After it lands, the web lane has no open items.
- Owner one-time step, if not done: Settings → Pages → Source "GitHub Actions". URL `https://kalharbi11.github.io/wirescape/`.

## Phase 1 sign-off needs
#271 owner results (A, B, C), 1H green (#256), and the contention benchmark (#274 plus owner D3). Then Compass runs Checkpoint A. The owner has deferred #271; nothing is asked of them until the 2A/2B/2C T2 lanes merge, so that one owner session covers Phase 1 and the first Phase 2 hardware checks.

## Retained rulings
- Electron main is the only automatic restarter. Imported documents round-trip unchanged (D3). New loadGraph uses D5; live edits use replan with bounded busy retry.
- Benchmark fields are render walltime. Missing metrics never count as zero.
- Known non-required reds: cortex-m7 (#159); `static` is informational.
- Tests and CI never open real audio, MIDI or LAN. Pi evidence comes only from the pi5 runner.

## Standing rules from the owner
- Don't plan agent assignment. Pi first; Windows (#266) after Test Session 1. Real-device tests are the owner's.
- No browser/kiosk product stage (D1). The web demo is a showcase build only (D14).
- Leads save usage: delegate mechanics, don't poll, ask the owner only for approvals and hardware checks.
