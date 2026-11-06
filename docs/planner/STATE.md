<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the planner reads this first)

Keep this under 60 lines. Rewrite it at the end of each planner session; history lives in git.

**Updated:** 2026-11-07 (plan v3 revised: standalone Electron app + engine sidecar; browser demo deferred).

## Where things stand
- Plan: `docs/PLAN.md` (phases 0–5, checkpoints A/B/C, decisions D1–D12, revised 2026-11-07).
- Phase 0 browser/kiosk demo: **stopped, deferred, not passed** (record: `docs/DEMO_CLASS.md` §1).
  #221 (demo patches) and #222 (launcher, `play --list-midi`) are draft PRs: review, then merge.
- Workflow v3 in `AGENTS.md` + `docs/WORKFLOW.md`. The pause on #29 ends when the plan PR merges.
- Phase 1 task files: P1_0, P1A, P1B (sidecar), P1C, P1D, P1E, P1F (Electron shell), P1G (owner UI fixes).

## Waiting on the owner
- `docs/DEMO_CLASS.md` §4–5 results: adapter tone, Six Essentials sound, M8 notes + CC21–28 per patch.
- Opening the plan PR (or asking the planner to) + `allow-protected`; re-enabling the Pi runner workflows.

## Next planner session
- Read the owner's §5 results. If audio fails beyond the adapter (§4 passes, §5a fails), write a
  diagnostic task for `play`/`portaudio_service` before P1B builds on it.
- Open the Phase 1 issues (one per section, `phase/1`); apply "Open work at resume" in PLAN.md.
- Review P1.0 (T2) as soon as it has a PR.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
