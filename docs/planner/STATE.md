<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (Claude reads this first at every check-in)

Keep this under 80 lines. Rewrite it at the end of each check-in; history lives in git.

**Updated:** 2026-10-29 13:45 New York.

## Check-in recipe (cheap)
1. Read this file. Don't read PLAN.md, WORKFLOW.md or task files unless an item below needs them.
2. Run one REST query for open PRs (`gh api repos/kalharbi11/wirescape/pulls`) and read the latest steward summary on #101. Don't sweep everything.
3. Act only on: PRs labelled or listed as waiting on T2-Claude, items under "Planner decisions pending", and queue refills when fewer than 3 `status/todo` items remain.
4. Rewrite this file, post the Codex queue on #29, and stop. No status essays.

## Goal
Jam checkpoint: J0–J9 (`docs/tasks/J_JAM_CHECKPOINT.md`).

## Where things stand
| Item | PR | State |
|---|---|---|
| J1 engine2 in app | #78 | merged |
| J2 polyphony | #79 | on `main`; file cap 24 accepted; needs ChatGPT pre-review, then merges via the gate |
| J3 headless playback | #80 | stacked on #79; acceptance red; retarget after #79 |
| J4 MIDI In | #81 | stacked, stale; refresh after #80 (cap 60 files) |
| J5 OSC In | #82 | stacked, stale; refresh after #81 (cap 55 files) |
| J6 Macro Osc | #77 | Pi acceptance green; needs ChatGPT pre-review |
| J7 CHAOS-lite | #124 | Pi acceptance green (520 µs, 0 alloc); conflict with base; waits on #79 |
| J8 author guide | — | see the task file |
| J9 user's jam session | — | user only |
| Static-check fix | #126 | Claude-approved (55bb36d), auto-merge on |
| Qt PATH test fix | #134 | auto-merge on |
| Unknown v2 modules | #133 (issue #132) | `static` red until #126 lands |
| Merge gate + rules | (this PR) | label-only |

## Planner decisions pending
- None.

## Runners
- Pi: `poll --minutes 330` daemon in wirescape-pi; nightly at 03:17. Sometimes offline when the Pi drops off Wi-Fi.
- Laptop: Python fixed 2026-10-29. Needs the `WIRESCAPE_TOKEN` "Issues: Read and write" permission on wirescape (user). Results go to #122.

## Standing rules from the user
- Conserve Claude usage: one check-in a day; the weekly limit resets Monday.
- Never @mention or start Codex; the user runs it. Post its queue on #29.
- Claude may enable auto-merge and post gate phrases on safe PRs.
