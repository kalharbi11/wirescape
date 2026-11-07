<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 11:30 UTC by Compass. **Pi first. Phase 2 runs beside Test Session 1. The web demo (D14) is a separate lane. Any owner failure in #271 preempts all work.**

## Lead split (owner, 2026-11-07)
- **Compass (platform):** everything except modules: Phases 1–5, the app, the engine lanes, the web demo, the board and STATE. Checks in every 4 h (owner-requested scheduled run).
- **Compass (modules):** Track S and the module lane (#304/#310, #314–#318, #305/#306/#307). Leaves this file's module lines to that session.
- Codex Lead runs every 150 min through 2026-11-09 06:00 UTC (owner-requested).

## Compass actions this session
- Board: closed #278, #281, #277 and #259 (their PRs had merged). Moved #294 (patch browser) and #295 (crash recovery) to `status/todo`, since their prerequisites are on main, and left a UX bar on #294.
- Closed #313 (a Codex Lead STATE built on a stale base); folded its live facts in below.

## Main blocker
- **#297**: Keel's #320 (test-only, +31/−5) replaces the #299 experiment. Every attempt keeps the full 3-block deadline, a bounded retry runs on hosted runners, and strict single-attempt mode is used for pi5 (`--max-gap-ms`). Needs a GPT/Codex T2 review, then merge. Until then, leads merge reviewed PRs whose only red is this test.

## In flight (who acts)
- #273 live replan in sidecar `00a5b24`, #274 bench summary `c6ed064`, #298 MIDI hotplug, #301 audio-loss recovery: Keel T2 review plus pi5, then merge. #274 unblocks #292/#296; #273 unblocks #287; #301 unblocks #293.
- #256 1H conformance: Keel rules on the 13-file budget and the guard red. Stale since 11-06 12:57. On the Phase 1 critical path.
- #300 keyboard default: retargeted to main and integrated at `e4e0ae8`, reviewed, ready-to-merge. Auto-merges on green.
- #311 (#306) conditional controls: Codex, draft, needs a new-head Web run plus an independent T1 review.
- #210 / #290 MIDI controls: Keel/Compass contract ruling pending.
- **Free now (Codex Lead to queue):** #294 patch browser, #295 crash-recovery offer.
- Still blocked: #287 (after #273), #292 and #296 (after #274), #293 (after #301).

## Web demo lane (D14)
- #309 merged (wasm engine, WebBridge, AudioWorklet, Pages workflow). Follow-up #312 W-web-2 (catalogue-filtered starters) is briefed for Codex. W-web-3..5 (telemetry parity, patch in URL, download) are not filed yet; file them once #312 lands.
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
