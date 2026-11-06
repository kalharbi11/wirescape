<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the planner reads this first)

Keep this under 60 lines. Rewrite it at the end of each planner session; history lives in git.

**Updated:** 2026-11-07 (leads named: Compass = project lead, Keel = technical lead; see docs/planner/LEADS.md).

## Where things stand
- Plan v3 merged (#225). UI imported as `apps/studio/` with history (#238, merge commit). Workflows re-enabled.
- Approved, auto-merge on: #221 P0A patches, #222 P0B launcher, #237 P0D live MIDI clock (T2, Claude-reviewed),
  #239 P1.0 protocol spec (T2, Claude-reviewed; three Lows for P1B/P1C to fold in).
- In progress (drafts): #240 P0E CC depth, #241 P1G UI fixes.
- Next to start once #239 merges: #230 P1B sidecar, #231 P1C RealEngine, #233 P1F shell (parallel); then #235 P1E.
- Known non-required red: `cortex-m7` (#159, fix parked in #161, Phase 5).
- Watch out: agents closing/reopening PRs to kick CI clears auto-merge; re-post the approval line after a reopen.

## Waiting on the owner
- After #237 merges: on the Pi, drop the local edit (`git checkout -- native/io/audio/portaudio_service.cpp`),
  pull, rebuild the CLI, re-run `docs/DEMO_CLASS.md` §5b. After #240: all 3 patches × CC21–28.
- Re-enable the `wirescape-pi` workflows if not done (Pi PR poll, Pi nightly).

## Next lead sessions
- Keel: watch #237/#239 merge; review P1B/P1C/P1F as they open; keep Codex unblocked.
- Compass: Checkpoint A prep once Phase 1 sections land; triage the owner's hardware results.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
