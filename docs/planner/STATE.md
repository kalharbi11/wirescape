<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (Claude reads this first at every check-in)

Keep this under 80 lines. Rewrite it at the end of each check-in; history lives in git.

**Updated:** 2026-10-31 afternoon, New York.

## Check-in recipe
Follow `docs/planner/CHECKIN.md`: routine Sonnet every 4 h, Opus for escalations and daily at 17:30.

## Deferred for Opus
(Sonnet adds `- #N sha7: question (since date time)`; Opus empties this list.)
- #206 `2706ce1`: approvals v3. Needs an independent ChatGPT T2 line, then the **owner's** Claude line (Claude wrote it).

## Goal
Owner Jam report #182 drives priorities until the owner retests and closes it. J9 #45 is the owner's session.

## Merged
J1, J2 poly, J4 MIDI #81, J6 Macro, J7 CHAOS #124, Clock, fullscreen, layout #145, controls #147, kiosk docs #169,
author guide #170, Macro catalogue #171, goldens #138, M2 dossier #191, tooltips #189, cable editing #190,
MIDI routing #193, M3-1 design #197 (#177 closed), event cable identity #199, M3-3 feedback design #202.

## Open work (queue comment 6540585199 on #29)
| PR | Issue | Owner | State |
|---|---|---|---|
| #203 | #201 transport focus | Codex | T1 reviewed; acceptance + ready label |
| #194 | #186 live editing | Codex | draft; part 2 park/edit/resume, T2 + Claude review |
| #195 | #188 launcher/update | Codex | Medium: `updates` vs stale GUI `current`; fake test, fresh T1 |
| #200 | #140 audio output | Codex | T2 (no Claude path); two ChatGPT passes |
| #82 | #72 OSC | Codex | draft; merge main + real-timestamp regression |
| #192 | #187 starter patches | UI | merge main, rerun studio_ui_tests, fresh T1 |
| #204 | #178 stereo pairs | UI | base branch merged via #199: retarget to main |
| #181 | #180 module guide | docs | draft; T1, link #170 |
| #175 | #173 gate fix | **owner** | clean T2 line at `f69bdba`; Claude co-authored, owner posts the Claude line or merges |
| #161 | #159 portability | parked | Cortex is non-required (authorization v2 item 4) |

## Next phase (below #182)
- #163 MIDI: extend `midi.to_cv` / `midi.cc_to_cv`. Codex, after #82.
- M3-1 implementation issue: write after #182 items land (design approved on #177).
- Later: #80 J3, #143 telemetry, `<details>` gate Low (from #175 review).

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
