<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 by Compass. **Pi first. Phase 2 runs beside Test Session 1. The web demo (D14) is a separate lane. Any owner failure in #271 preempts all work.**

## Compass actions this session
- Merged #279 (patch folder) and #280 (Codex Lead STATE) directly. In both, the only red was the #297 Windows timing flake, which is unrelated to their diffs.
- #282 (autosave): marked ready, labelled, `Claude: approved for auto-merge 5be3b59`.
- Posted a recommendation on #297 for Keel: keep the 3-block/20 ms assertion on Linux, ARM64 and pi5. On hosted Windows, run it in a functional mode (commit through live or D5, gen increments, no fault) with a visible skip log.
- **New D14 web demo** (owner request): #308 / draft PR #309, built by Compass. Task `docs/tasks/W_WEB_DEMO.md`.

## Main blocker
- **#297 / #299**: the live-replan Windows timing test goes red on any PR with a required Windows job. It needs Keel's ruling (see the Compass comment on #297). Until then, leads merge reviewed PRs whose only red is this test.

## In flight (who acts)
- #273 live replan in sidecar, at `00a5b24`: Keel re-review plus pi5, then merge.
- #274 bench summary, at `c6ed064`: Keel re-review plus pi5, then merge. Unblocks #292 and #296.
- #298 MIDI hotplug and #301 audio-loss recovery: need a Keel T2 review plus pi5.
- #256 1H conformance: Keel rules on the 13-file budget and the guard red. Stale since 11-06 12:57.
- #300 keyboard default: retarget after #283 (merged), then full Web run.
- #304 spectral S1-1: needs a T1 review from another session plus a Pi probe.
- #210 / #290 MIDI controls: Keel/Compass contract ruling pending.
- Blocked queue (start when the prerequisite merges): #287 (after #273), #292 and #296 (after #274), #293 (after #291), #294 (after #279 and #282), #295 (after #282).

## Web demo lane (D14)
- PR #309: `native/web` (wasm engine C API mirroring `Session::execution`), `apps/studio/src/web` (WebBridge for protocol v0; RealEngine unchanged), `public/web/ws-processor.js` (AudioWorklet), `.github/workflows/web-demo.yml` (native tests, Emscripten 3.1.74 wasm build, Playwright smoke, Pages deploy on main).
- First CI run is the first real compile: the cloud container can't fetch deps or install emsdk. Compass is subscribed to #309 and fixes CI.
- Codex follow-ups after merge: W-web-2 (hide MIDI starters and library entries), W-web-3 (telemetry parity), W-web-4 (patch in URL), W-web-5 (download patch).
- Owner one-time step: Settings → Pages → Source "GitHub Actions". The URL will be `https://kalharbi11.github.io/wirescape/`.

## Phase 1 sign-off needs
#271 owner results (A, B, C), 1H green (#256), the contention benchmark (#274 plus owner D3). Then Compass runs Checkpoint A.

## Retained rulings
- Electron main is the only automatic restarter. Imported documents round-trip unchanged (D3). New loadGraph uses D5; live edits use replan with bounded busy retry.
- Benchmark fields are render walltime. Missing metrics never count as zero.
- Known non-required reds: cortex-m7 (#159); `static` is informational.
- Tests and CI never open real audio, MIDI or LAN. Pi evidence comes only from the pi5 runner.

## Standing rules from the owner
- Don't plan agent assignment. Pi first; Windows after Test Session 1. Real-device tests are the owner's.
- No browser/kiosk product stage (D1). The web demo is a showcase build only (D14).
- Leads save usage: delegate mechanics, don't poll, ask the owner only for approvals and hardware checks.
