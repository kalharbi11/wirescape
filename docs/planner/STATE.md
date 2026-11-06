<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 by Keel (check-in: cleared the #240 stall, merged the ready queue, ruled on #209).

## Where things stand
- Merged earlier: #237 P0D, #238 P1A, #239 P1.0 protocol, #241 P1G, #243 lead names.
- **Merged this session (Keel direct, protected-path leads):** #244 (protocol §5 ID-namespace ruling + #239 Lows), #245 (`static` cppcheck `--inline-suppr`, so vendored nlohmann stops failing the informational job), #246 (combined-lead planner docs). `#234` closed (done via #241).
- **#240 P0E — unstuck.** Was waiting on a retarget; base is now `main` and head `56daf7d` merges current main. The six task files are byte-identical to the reviewed head `57c6f63`, so my T1 review carries; I re-posted the approval line for `56daf7d` and **armed native auto-merge**. It lands when the in-flight required CI goes green. (`portability`/`cortex-m7` = #159, `static` fixed by #245 — neither required.)
- **#242 P1D codec** — my left-channel-only High is fixed (PatchDoc now routes bus 0+1; six bundled Qt starters authored stereo in `starters.ts`; conformance + codec tests updated). Scope 15 files, within WORKFLOW §5's +50% allowance. Still draft, exact-head CI in flight. **Not waiting on Keel** — Codex Lead posts the approval line on green. Pi not needed.
- **Phase 1 parallel tracks, ready now:** #230 P1B sidecar (in-progress), #231 P1C RealEngine (todo), #233 P1F shell (todo), #242 P1D (in flight). #235 P1E after #230 + #233.
- Known non-required red everywhere: `cortex-m7` (#159; fix parked in #161, Phase 5).

## Rulings this session
- **#209 live-edit Stop lifted (Keel).** The "MIDI source-subscription / reused-ID" decision the builder waited on is D7 as refined by #244: bounded control-thread subscription refresh with audio live, held notes on a removed source released, IDs never reused within the loaded document (`loadGraph` starts a fresh namespace + `gen`). #209's choke + provenance filtering already match. Moved off `blocked` → `in-progress`. It is **Phase 2 (2A)** work — remaining items (hosted sanitizers, the #200 fake-audio Studio test, then Keel T2) are not lead decisions; triage at Checkpoint A.

## Waiting on the owner
- **One Pi session once #240 merges (P0F).** Drop the old local edit, update to main, rebuild, then `docs/DEMO_CLASS.md` §3 + §5b for all 3 patches × CC21–28:
  `cd ~/wirescape-demo && git checkout -- native/io/audio/portaudio_service.cpp && git fetch origin main && git checkout -B main origin/main && python3 tools/dev.py build --headless`
  Post pass/fail per patch on #227.
- Nothing else. `wirescape-pi` workflows are active.

## Next lead sessions
- Keel: confirm #240 merged; cross-family **T2-review #230 (P1B)** and **#242 (only if its final diff drifts)** when green/ready; review #231/#233 PRs as they open; keep Codex unblocked.
- Compass: Checkpoint A prep once #230/#231/#233 land; triage the owner's P0F result.
- Stale drafts parked (triage at Checkpoint A, not before): #82 (OSC, P3), #161 (#159, P5), #181 (blocked docs T1 — needs one T1 review), #209 (→2A), #210 (P2).

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
