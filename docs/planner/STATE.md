<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (Claude reads this first at every check-in)

Keep this under 80 lines. Rewrite it at the end of each check-in; history lives in git.

**Updated:** 2026-10-31 New York.

## Check-in recipe (cheap)
1. Read this file. Skip PLAN.md, WORKFLOW.md and task files unless an item below needs them.
2. One REST query each: open PRs, issues labelled `needs-planner`, the last 3 comments on #29. Tail, never read whole logs.
3. Act only on `needs-planner` items, T2-Claude gates and queue refills (fewer than 3 `status/todo`).
4. Rewrite this file (small PR, auto-merge), post one queue comment on #29, stop.

## Goal
Owner Jam report #182 drives priorities until the owner retests and closes it. J9 #45 is the owner's session.

## Merged
J1, J2 poly, J4 MIDI #81, J6 Macro, J7 CHAOS #124, Clock, fullscreen, layout #145, controls #147,
kiosk docs #169, author guide #170, Macro catalogue #171, goldens #138, M2 dossier #191, tooltips #189 (#185).

## Jam report #182 (top of the queue)
| Issue | PR | Owner | State |
|---|---|---|---|
| #183 cable editing | #190 `5d8232f` | Codex | T1 pre-review: no open High; Codex settles the QPointer analyzer note, then ready label + gate |
| #184 MIDI routing | #193 `f2fc382` | Codex | T2 second pass: High (`--ready-report` alone can enable saved MIDI consent); fix + fake test, then fresh T2 |
| #185 tooltips | #189 | UI | merged |
| #186 live editing | #194 (draft, stacked on #190) | Codex | after #190 merges; retarget |
| #187 starter patches | #192 `14c25cb` | UI | T1 pre-review: Highs (Windows UI load/layout/static on this head) |
| #188 launcher/update | #195 (draft) | Codex | CI fixes in progress |

## Next phase (below #182)
- #163 MIDI: extend existing `midi.to_cv` / `midi.cc_to_cv` (no new modules). Codex, after #184.
- #177 M3-1 parameter-edge design: **owner approved doing it**; design PR stays `milestone-review`, Claude gates.
- #178 M3-2 stereo pairs: UI agent after #187 (controller change authorized; see #178 handoff).
- #179 M3-3 feedback design: docs agent, not prioritized.
- Later: #80 J3, #82 J5 OSC (cap 50), #161 portability (parked), #175 gate fix, #143, #140.

## Planner decisions pending / waiting on Claude
- #175 / #173 merge-gate fix: task scope + protected-path line written on #173 (2026-10-31). Codex fixes the
  indented-code High; Claude wrote #175, so the **reviewer's** independent line plus a Claude line on the new head.
- #161: parked. Cortex is a non-required check (authorization v2 item 4); revisit after #182.
- #177 design review when its PR appears (T2-Claude, Opus).

## Standing authorization v2 (#29, comment 6534860663)
Valid until revoked. Formatting never stops; +50% size; offscreen/fake evidence stands in for devices;
inherited non-required checks don't block; ChatGPT reviewer posts T1/T2 gate lines. Still stops: ABI,
patch-format/ID changes, new deps, test removal, looser thresholds, protected paths, real devices, force-push.

## Agents
- Codex (owner starts it; reads #29), ChatGPT reviewer + steward (hourly), UI agent, docs agent
  (prompts: `docs/planner/CHATGPT_AGENTS.md`). Sonnet routine every 4 h: quick approvals, labels `needs-planner`.
- Merge gate: `tools/merge_gate.py` + `.github/workflows/merge_gate.yml` auto-merge on `status/ready-to-merge`
  plus a standalone gate line for the current head.

## Runners
- Pi `pi5` via `kalharbi11/wirescape-pi` (poll daemon, nightly). Laptop results on #122; Pi/steward summaries on #101.

## Standing rules from the owner
- Minimal owner involvement; agents keep moving. Never @mention Codex.
- Claude usage is scarce: REST only, tail logs, delegate searches, one #29 comment per session.
- Real-device tests are the owner's; Pi and laptop run offline tests only.
