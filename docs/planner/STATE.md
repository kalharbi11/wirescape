<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 by Compass (project check-in; plan refreshed; board tidied). Keel was mid-session on #247/#249/#253.

## Where things stand
- **Phase 0:** code-complete. 0D #237 and 0E #240 merged. Only 0F (owner Pi re-test) is left; #227 stays open to collect it.
- **Phase 1 merged:** 1.0 #239, 1A #238, 1D #242, 1G #241, plus the protocol ID-namespace ruling #244.
- **#249 P1C RealEngine:** Keel T1-reviewed, fixed the crash-restart loop (main owns crash restarts) and armed auto-merge at `706e333`. It lands on green; nothing needed.
- **#253 P1F shell (draft):** rebase on #249 once it merges (conflict in `apps/studio/src/app/App.ts`). Keel's notes: renderer-requested restart must not reset `failures`, and the launcher passes `--confirm-audio --confirm-midi`.
- **#247 P1B sidecar (draft, T2, critical path):** red on real bugs. Keel's single checklist on #247: merge main, `hello` before render, the Windows fake-MIDI disable hang, static findings, and the D5 fade (ruled: ≤10 ms linear master-gain ramp each side, gap ≤150 ms).
- **#250 conformance harness** is now plan item **1H**; it starts once 1B and 1C have their basic shapes and must be green before 1E signs off.
- **#235 P1E** after 1B + 1F.
- Known non-required red everywhere: `cortex-m7` (#159; fix parked in #161, Phase 5).

## Decisions this session (Compass)
- **D13 sidecar contingency:** if #247 is red again after Keel's checklist, split it into 1B-1 (protocol, null sink, fake MIDI, fade: everything CI can prove) and 1B-2 (PortAudio + real MIDI devices). No third attempt at the full L-size PR.
- Plan "Now" rewritten; 0E, 1D and 1G ticked; 1H added.
- Board: closed #70 (J3 done, superseded by 1E/2E) and #140 (Qt Studio silence; #200's semantics carry into 2C). Phase labels on parked items: #188 → 4A; #72/#94/#143/#178 → 3; #163/#164/#186/#209/#210 → 2; #64/#91/#159/#161 → 5. Created labels `phase/3`–`phase/5`.
- Electron (D1) reconfirmed; no change.

## Waiting on the owner
- **P0F Pi re-test, now unblocked.** Drop the old local edit, update to main, rebuild, then `docs/DEMO_CLASS.md` §3 + §5b for all 3 patches × CC21–28:
  `cd ~/wirescape-demo && git checkout -- native/io/audio/portaudio_service.cpp && git fetch origin main && git checkout -B main origin/main && python3 tools/dev.py build --headless`
  Post pass/fail per patch on #227.

## Next lead sessions
- Keel: drive #247 through the checklist (or apply D13); T2-review #247 when green; review #253 after its rebase; review #250 when it opens.
- Compass: triage the owner's P0F result on #227; write the Checkpoint A agenda once 1B + 1F are merged; decide whether 1E needs a Windows smoke test before the Pi.
- Stale drafts parked (triage at Checkpoint A, not before): #82 (OSC, P3), #161 (#159, P5), #181 (docs T1, needs one review), #209 (→2A), #210 (P2).

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
