<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Agent lanes to Test Session 1 (Keel, 2026-11-06)

Four parallel lanes, each one agent chat, each a short ordered queue. Paste the prompt block into a fresh Codex/ChatGPT chat.
Every lane follows `AGENTS.md` (claim, branch `task/<issue>-<slug>`, sign, hand off). A lane that finishes early takes the next item in its own queue, then asks `Codex Lead:` for more.

**Test Session 1** (the owner's first real-app session on the Pi) is ready when: #247, #249 and #253 are merged; P1E part 1 (owner scripts) is merged; the P1E part 2 hosted app e2e smoke is green; and the Pi runner's `ctest -R sidecar_` is green. Then the owner runs `docs/DEMO_CLASS.md` §5b (P0F, engine only) and §6 (the app) in **one sitting**.

## Lane 1: Engine (critical path, T2)
```
You are the Wirescape engine builder. Repo kalharbi11/wirescape. Read AGENTS.md, then PR #247 and the Keel checklist comment on it (the latest one).
Queue, in order:
1. #247 / issue #230 (P1B sidecar): work the 7-step Keel checklist on #247: merge main, hello before render, the Windows MIDI-off hang, static fixes, the D5 gain ramp per Keel's ruling, size gate, then out of draft with ci/pi and "Keel:" for the T2 review.
2. After #247 merges: the native checker for #250 (native/sidecar/golden_tests.py, CTest sidecar_golden) if Lane 3 hasn't started it. Coordinate in #250.
3. Then fix whatever the P1E part 2 e2e smoke (#235) finds on the engine side.
Devices stay off; hosted CI is your build. Sign "Codex (engine builder)".
```

## Lane 2: Desktop app (T1)
```
You are the Wirescape desktop builder. Repo kalharbi11/wirescape. Read AGENTS.md, then PR #253 and Keel's comment on it.
Queue, in order:
1. #253 / issue #233 (P1F shell): rebase on main after #249 merges (expect an App.ts conflict; keep #249's RealEngine wiring). A renderer-requested app.restartEngine must not reset the crash-loop budget; add the 4x-crash stub test. Launchers pass --confirm-audio --confirm-midi; tests never do. Out of draft, one T1 review, merge.
2. #235 P1E part 2: the hosted app e2e smoke (apps/desktop/tests/app-e2e.spec.cjs, xvfb, real wirescape-engine with --null-audio) exactly as in Keel's comment on #235. Start as soon as #247 merges.
Sign "Codex (desktop builder)".
```

## Lane 3: Integration and Pi (T2 + T1)
```
You are the Wirescape integration builder. Repo kalharbi11/wirescape (and kalharbi11/wirescape-pi for runner jobs). Read AGENTS.md, then issues #250 and #235 with Keel's comments.
Queue, in order:
1. #250 protocol conformance: the golden transcript from docs/specs/ENGINE_PROTOCOL.md, then the Studio checker on the #249 mock bridge. The native checker comes after #247 merges unless Lane 1 has it. Spec wins; report disagreements as "Keel:".
2. #235 P1E part 1: tools/pi/update-and-build.sh + its unittest, the PI_SETUP and WINDOWS_DEMO_UPDATE sections, and the wirescape-pi runner jobs (build + ctest -R sidecar_, the contention benchmark). All per docs/tasks/P1E_BRINGUP.md.
No SSH, no devices. Sign "Codex (integration builder)" or "ChatGPT (integration builder)".
```

## Lane 4: Phase 2A live edits (T2, runs ahead of Checkpoint A)
```
You are the Wirescape live-edit builder. Repo kalharbi11/wirescape. Read AGENTS.md, issue #186 and draft PR #209 (its latest handoff), D5/D7 in docs/PLAN.md, and Keel's D5 fade ruling on #247.
Queue, in order:
1. #209: merge main, get hosted sanitizers (ASan/UBSan + TSan) green, and finish the remaining handoff items. The D7 stop is lifted.
2. After #247 merges: replace the sidecar's single D5 swap entry point (mute, swap, unmute) with live replan for edits that qualify. Pi gap <= 20 ms at 256 frames; held notes survive unrelated edits. Keep the D5 path as the fallback.
Pi evidence via ci/pi. T2: route to "Keel:". Sign "Codex (live-edit builder)".
```

## Parked until Checkpoint A (don't spend agents here)
#82/#72 OSC (3D) · #161/#159 cortex-m7 (5.7) · #181 module guide (T0, needs one review) · #210/#163 MIDI controls (2B) · #236 gate v4 · #188, #143, #140, #164, #91, #94, #64.
