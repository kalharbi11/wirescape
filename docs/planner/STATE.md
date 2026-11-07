<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 by Codex Lead; Compass's platform/module lane split retained. **Pi first. Phase 2 runs beside Test Session 1. The web demo (D14) is a separate lane. Any owner failure in #271 preempts all work.**

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
- #273 and #274 are merged (verified). #301 still needs its audio-recovery follow-through.
- #298 is closed but reports unmerged; inspect its replacement/closure before assuming hotplug shipped.
- #297 is resolved by #320; retain strict Pi evidence requirements.

## In flight (who acts)
- #292 load/dropouts and #296 fake-device Pi soak are now unblocked by #274; queue comments posted.
- #287's topology prerequisite is done, but its raw-CC observation seam still needs the recorded ruling.
- #301 audio recovery gates #293 device details; retain fake-device/Pi evidence rules.
- #256 1H conformance: approved at `01305a3`, auto-merges on green. If web goes red on the merged head, fix it with a small change; don't re-scope.
- #300 and #311 merged.
- #210 / #290 MIDI controls: Keel/Compass contract ruling pending.
- #324 crash recovery merged (verified).
- #294 patch browser: draft #331 at 06c52dc, 10 files/~270 lines; filesystem/syntax checks pass.
  Four Web scenarios + Electron boundaries added; exact-head full Web/Electron and T1 review pending.
  Local typecheck unavailable (tsc missing after proxy HTTP503). No physical-device evidence claimed.

## Web demo lane (D14)
- Live at `https://kalharbi11.github.io/wirescape/` (Pages is on). #309 and #321 merged (`e766a27`): the demo has playable starters only, cable values and the Output scope, a SHARE link (`#p=`), and SAVE/OPEN files.
- Every main push runs `web-demo.yml`: native tests, the wasm build, Playwright, deploy, then `live` (the same tests against the published site). Agent containers can't reach github.io, so `live` is the deployed-site evidence.
- Ideas, not queued: per-module effective/phase/envelope telemetry, a short first-patch tip on the splash, a phone/touch pass.

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
