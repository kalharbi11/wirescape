<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the planner reads this first)

Keep this under 60 lines. Rewrite it at the end of each planner session; history lives in git.

**Updated:** 2026-11-06 (plan v3: web UI integration, workflow v3).

## Where things stand
- Plan v3 is in `docs/PLAN.md` (phases 0–5, checkpoints A/B/C, decisions D1–D12).
- Workflow v3 is in `AGENTS.md` + `docs/WORKFLOW.md`: any agent picks up any item; claim → sign → hand off.
  Scheduled agent check-ins are retired. The old queue comment on #29 (6540585199) is retired.
- The pause on #29 ends when the plan PR merges (the owner or planner posts "Project resumed" on #29).
- Phase 0 = class demo 2026-11-07 (`docs/DEMO_CLASS.md`). Phase 1 = bridge (host + RealEngine), Pi first.

## Waiting on the owner
- CC numbers of the controller knobs for the demo patches (P0A).
- Exact MIDI port names and the USB audio device name on the Pi (`wirescape play --list-devices`, `aconnect -l`).
- The Pi UI test results (resolution, DPR, FPS feel) for the UI repo's OPEN-5 note.
- `allow-protected` on the plan PR (it edits AGENTS.md, CLAUDE.md and docs/PLAN.md).

## Next planner session
- After the demo: read the debrief comment, open the Phase 1 issues (one per section, `phase/1`), apply the
  "Open work at resume" table in PLAN.md, and write the Phase 2 task files.
- Review 1.0 (protocol spec, T2) as soon as it has a PR.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first; touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
