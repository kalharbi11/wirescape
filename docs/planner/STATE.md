<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 by Keel + Compass (one combined session). Lead prompts were revised with a shared "How you work" block (`docs/planner/LEADS.md`).

## Where things stand
- Merged: #237 P0D live MIDI clock, #238 P1A UI import, #239 P1.0 protocol spec (Keel merged it directly; its only red was a GitHub 502), #243 lead names.
- Auto-merge pending green (approval lines posted):
  - #240 P0E CC depth at `57c6f63`.
  - #241 P1G UI fixes at `cb86d63`. Retargeted to main. Keel fixed a High: MIDI In now saves `endpoint` + `system_name`, which native reads.
  - If either goes red, read the log and fix or hand off. Watch #241's `review-artifacts.spec.ts`: it failed locally with `TargetClosedError` and passed in hosted CI before.
- Waiting on its builder: #242 P1D codec. Keel's High: Output routes go to bus 0 only, so UI patches are left-channel only. The exact fix is posted on the PR; the label is in-progress.
- Need a Codex/GPT T2 review, then merge (both Keel-authored, protected paths labelled):
  - #244: protocol §5 module ID namespace (ruling below) plus the #239 Lows.
  - #245: `static` cppcheck `--inline-suppr`, so vendored nlohmann stops failing the informational job.
- Unblocked, ready to start in parallel: #230 P1B sidecar, #231 P1C RealEngine, #233 P1F shell. Each got a short brief. #235 P1E comes after #230 + #233.
- Board cleaned: closed #226 (done), #219 and #220 (done), #223 and #224 (duplicates).
- Known non-required red: `cortex-m7` (#159; fix parked in #161, Phase 5).

## Rulings this session
- **Module ID namespace (Keel, refines D7):** "never reused" covers incremental edits within the loaded document. A successful `loadGraph` starts a fresh namespace with a new `gen`, so switching between the demo patches is valid. Old work is drained and stale-`gen` replies are ignored. Recorded in #230, `ENGINE_PROTOCOL.md` §5 (#244) and PLAN D7.
- **Owner Pi checks are batched (Compass):** one Pi session after #240 merges (P0F) instead of one per fix.

## Waiting on the owner
- One Pi session once #240 is merged (P0F). Drop the old local edit, update to main, rebuild, then follow `docs/DEMO_CLASS.md` §3 and §5b for all 3 patches × CC21–28:
  `cd ~/wirescape-demo && git checkout -- native/io/audio/portaudio_service.cpp && git fetch origin main && git checkout -B main origin/main && python3 tools/dev.py build --headless`
  Post pass or fail per patch on #227.
- Nothing else. The `wirescape-pi` workflows are already active.

## Next lead sessions
- Keel:
  - Confirm #240 and #241 merged.
  - Re-review #242 after the bus fix, then approve.
  - Merge #244 and #245 once a Codex review lands.
  - T2-review #230 when it opens.
- Compass: Checkpoint A prep once #230, #231 and #233 land; triage the owner's P0F result.
- Stale drafts left as-is (old plan): #82, #161, #181, #209 (becomes 2A, per D7), #210. Triage at Checkpoint A, not before.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
