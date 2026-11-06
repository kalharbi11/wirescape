<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Agent lanes to Test Session 1 (Keel, 2026-11-06, second pass)

Six parallel lanes (5 and 6 are new and independent of the critical path), each one agent chat, each a short ordered queue. Paste the prompt block into a fresh Codex/ChatGPT chat.
Every lane follows `AGENTS.md` (claim, branch `task/<issue>-<slug>`, sign, hand off). A lane that finishes early takes the next item in its own queue, then asks `Codex Lead:` for more.

**Test Session 1** (the owner's first real-app session on the Pi) is ready when: #247, #249 and #253 are merged; P1E part 1 (owner scripts) is merged; the P1E part 2 hosted app e2e smoke is green; and the Pi runner's `ctest -R sidecar_` is green. Then the owner runs `docs/DEMO_CLASS.md` §5b (P0F, engine only) and §6 (the app) in **one sitting**.

## Lane 1: Engine (critical path, T2)
```
You are the Wirescape engine builder. Repo kalharbi11/wirescape. Read AGENTS.md, then PR #247 and Keel's latest comment on it.
Queue, in order:
1. #247: Keel pushed 19bb2f2 and reviewed the rest. Do a cross-family COMMENT review of 19bb2f2 only (about 30 lines of logic plus formatting). If windows-x64 is still red at protocol_tests.py:160 (fake MIDI off never replies), fix that one item: capture the child's stderr and check the setMidiEnabled stop path. Link the Pi ctest -R sidecar_ run.
2. #258: null-sink render histogram + --bench-summary stderr line (after #247 merges).
3. #250 native checker: native/sidecar/golden_tests.py, CTest sidecar_golden, reading tests/protocol/golden_v0.ndjson from #256.
Devices stay off; hosted CI is your build. Sign "Codex (engine builder)".
```

## Lane 2: Desktop app (T1)
```
You are the Wirescape desktop builder. Repo kalharbi11/wirescape. Read AGENTS.md, then Keel's comments on #235.
Queue: #235 P1E part 2, the hosted app e2e smoke (apps/desktop/tests/app-e2e.spec.cjs, xvfb, real wirescape-engine with --null-audio, run from the existing web job). Start as soon as #247 merges; until then build it against the #247 branch binary.
Then #260 (Pi autostart, realtime-priority doc, boot-to-sound log) if Lane 6 hasn't taken it.
Sign "Codex (desktop builder)".
```

## Lane 3: Integration (T2 + T1)
```
You are the Wirescape integration builder. Repo kalharbi11/wirescape (and kalharbi11/wirescape-pi for runner jobs). Read AGENTS.md, then PRs #256 and #257 with Keel's rulings.
Queue, in order:
1. #256: fix PatchDoc per Keel's T02 ruling (imported docs round-trip unchanged; authored content may carry fields). Keep your T02 assertion. Ask "Keel:" for the T2 review.
2. #257: out of draft when green (T1, merges first); then the wirescape-pi runner PR parsing the #258 bench line (BLOCKED until #258 lands).
No SSH, no devices. Sign "ChatGPT (integration builder)".
```

## Lane 4: Phase 2A live edits (T2)
```
You are the Wirescape live-edit builder. Repo kalharbi11/wirescape. Read AGENTS.md, issue #186 and PR #209 (its latest handoff), and Keel's D5 ruling on #247.
Queue: 1. #209: fix the static clang-tidy items (engine2.cpp:865 strcmp, live_replan_tests.cpp), rerun CI + ci/pi, route to "Keel:".
2. After #247 merges: behind the sidecar's single D5 swap entry point, use live replan for edits that qualify (gap <= 20 ms at 256 frames) and keep D5 as the fallback.
Sign "Codex (live-edit builder)".
```

## Lane 5 (new): Modules (T1)
```
You are a Wirescape module builder. Repo kalharbi11/wirescape. Read AGENTS.md, issue #259, docs/SDK_AUTHOR_GUIDE.md and native/sdk/examples/ (Claude Code: use the new-module skill).
Queue: #259: register Step Sequencer and Random Source as built-in engine modules with the IDs unchanged, regenerate the catalogue, flip the two refused starters in ui_patch_conformance to load+render, and add render smoke tests. ci/pi.
Any ID mismatch with the UI is a stop: ask "Keel:". Sign "Codex (module builder)".
```

## Lane 6 (new, after #253 merges): Pi appliance (T1)
```
You are the Wirescape Pi appliance builder. Repo kalharbi11/wirescape. Read AGENTS.md, issue #260, docs/PI_SETUP.md, and apps/desktop/ + tools/pi/ on main.
Queue: #260: install-autostart.sh with a unittest, the realtime-priority owner doc, and boot-to-sound stderr timing with a stub-engine test. No system files edited by scripts, no devices in tests.
Sign "Codex (Pi appliance builder)".
```

## Parked until Checkpoint A (don't spend agents here)
#82/#72 OSC (3D) · #161/#159 cortex-m7 (5.7) · #181 module guide (T0, needs one review) · #210/#163 MIDI controls (2B) · #236 gate v4 · #188, #143, #164, #91, #94, #64. (#140 and #70 closed by Compass as superseded.)
