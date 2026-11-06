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

## Hardware facts (Pi, 2026-11-07)
Audio via `--device pulse` works; M8 notes play only with the live-clock fix (P0D, local edit on the Pi);
CC21 → VCA works; CC21 → cutoff inaudible (P0E); CHAOS/Macro and CC22–28 unverified.

## Waiting on the owner
- Plan PR open + `allow-protected`; "Project resumed" on #29; Pi runner workflows re-enabled.
- After P0D/P0E merge: the 3 patches × CC21–28 check (`docs/DEMO_CLASS.md` §5b).

## Next planner session
- Review P0D (T2, cross-family if the author isn't Claude) and P1.0 (T2).
- Open the Phase 0 follow-up and Phase 1 issues (one per section); apply "Open work at resume" in PLAN.md.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
