<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 20:30 UTC by Compass (platform check-in). **Pi first. Phase 2 runs beside Test Session 1. The web demo (D14) is a separate lane. Any owner failure in #271 preempts all work.**

## Lead split (owner, 2026-11-07)
- **Compass (platform):** everything except modules: Phases 1–5, the app, the engine lanes, the web demo, the board and STATE. Checks in every 4 h (owner-requested scheduled run).
- **Compass (modules):** Track S and the module lane (#304/#310, #314–#318, #305/#306/#307). Leaves this file's module lines to that session.
- Codex Lead runs every 150 min through 2026-11-09 06:00 UTC (owner-requested).

## Compass actions (20:13 check-in)
- Merged since 16:12: #320 (#297 fix), #256 (1H), #273 (2A-2), #274 (1E-b), #330 (2B-c hotplug, which replaced #298), #324 (2D-d recovery), #321 (web extras). **All Phase 1 code is on main.**
- #271: posted the refreshed feature table and told the owner the session is ready to run (0, A, B, C, D3 = minimum for sign-off).
- #301: Keel's two Mediums have had no builder for 9 h. Asked Codex Lead to take it over, with the exact fix.
- #331 patch browser: independent T1 review found no High/Medium. Two Lows (preload request rebuild, uncaught `reset` in `loadBundledPatch`) should be fixed before ready; after that, any lead or Codex Lead may approve on green.

## Main blocker
- **Owner Test Session 1 (#271)** is now the only Phase 1 sign-off item. Then Compass runs Checkpoint A.
- Phase 2 engine: #301 (2C-a) needs a builder for Keel's Mediums, and it gates #293.

## In flight (who acts)
- #301 audio recovery: Codex builder fixes, then Keel re-review plus pi5.
- #331 patch browser: Codex fixes the two Lows, then approve.
- #292 load/dropouts and #296 fake-device Pi soak: unblocked by #274, queued by Codex Lead.
- #287 MIDI Learn: topology prerequisite done; raw-CC observation seam ruling needed (Keel).
- #210 / #290 MIDI controls: Keel/Compass contract ruling pending.
- Phase 2 left after these: 2B-b Learn, 2B-e controls, 2C-b/c, 2F-b soak. Then Checkpoint B.

## Web demo lane (D14)
- Live at `https://kalharbi11.github.io/wirescape/` (Pages is on). #309 and #321 merged (`e766a27`): the demo has playable starters only, cable values and the Output scope, a SHARE link (`#p=`), and SAVE/OPEN files.
- Every main push runs `web-demo.yml`: native tests, the wasm build, Playwright, deploy, then `live` (the same tests against the published site). Agent containers can't reach github.io, so `live` is the deployed-site evidence.
- Ideas, not queued: per-module effective/phase/envelope telemetry, a short first-patch tip on the splash, a phone/touch pass.

## Phase 1 sign-off needs
Only the owner's #271 results remain (A, B, C plus D3 contention). 1H (#256) and the benchmark tooling (#274) are merged. Then Compass runs Checkpoint A (about 30 min on the Pi with the M8).

## Retained rulings
- Electron main is the only automatic restarter. Imported documents round-trip unchanged (D3). New loadGraph uses D5; live edits use replan with bounded busy retry.
- Benchmark fields are render walltime. Missing metrics never count as zero.
- Known non-required reds: cortex-m7 (#159); `static` is informational.
- Tests and CI never open real audio, MIDI or LAN. Pi evidence comes only from the pi5 runner.

## Standing rules from the owner
- Don't plan agent assignment. Pi first; Windows (#266) after Test Session 1. Real-device tests are the owner's.
- No browser/kiosk product stage (D1). The web demo is a showcase build only (D14).
- Leads save usage: delegate mechanics, don't poll, ask the owner only for approvals and hardware checks.
