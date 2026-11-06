<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 by Keel (overview, merged #242, fixed and approved #249, combined lanes to Test Session 1), then Compass (check-in, plan refresh, board).

## Where things stand
- **Merged:** #237 P0D, #238 P1A, #239 P1.0, #240 P0E, #241 P1G, #242 P1D (Keel direct, `fcb8fab`), plus lead docs #243–#251.
- **#249 P1C RealEngine:** Keel T1 review done. Keel pushed `1fa65c3` (main owns crash restarts: RealEngine no longer sends `app.restartEngine` on an unexpected exit, which would have reset main's crash-loop budget forever) and merged main (`706e333`). Approved; `status/ready-to-merge`; auto-merge on green.
- **#247 P1B sidecar: the critical path.** Red for real: `hello` is gated behind render (`protocol_tests.py:209`) and Windows MIDI-off never replies (`:160`), plus static/format. The D5 fade is ruled (master gain ramp, linear ≤10 ms each side, gap ≤150 ms). One ordered 7-step checklist is on #247. Needs a builder now.
- **#253 P1F shell:** draft, builder working. Keel note: don't reset `failures` on a renderer restart; rebase after #249 (App.ts conflict); launchers pass `--confirm-*`.
- **#252 closed:** #242's `ui_catalogue_sync` CTest is already the required drift gate.
- **#235 P1E:** starts now. Part 1 = owner scripts + Pi runner jobs; part 2 (added) = a hosted Electron + real-engine e2e smoke, the gate before the owner's time.
- **#250 conformance:** starts now (Studio checker first, native after #247).
- Known non-required red: `cortex-m7` (#159).

## Compass decisions (2026-11-06 check-in)
- Agreed: P0F is folded into Test Session 1; PLAN "Now" and 0F updated. 0E, 1D, 1G ticked; #250 is plan item **1H**.
- **D13 sidecar contingency:** if #247 is red again after Keel's checklist, split it into 1B-1 (protocol, null sink, fake MIDI, fade: everything CI proves) and 1B-2 (PortAudio + real MIDI devices). No third attempt at the full L PR.
- Electron (D1) reconfirmed. Board: closed #70 (done) and #140 (Qt; semantics → 2C); `phase/2`–`phase/5` labels on parked items (labels 3–5 created).

## Lanes (copy-ready prompts in `docs/planner/LANES.md`)
1. Engine: #247 → #250 native checker.  2. Desktop: #253 → P1E e2e smoke.
3. Integration: #250 → P1E scripts + Pi runner.  4. Live edits: #209 (2A) → replace the D5 swap with live replan.

## Rulings this session
- **Crash restart ownership (Keel):** Electron main is the only automatic restarter. The renderer sends `app.restartEngine` only for an explicit user action. A renderer request never resets the crash-loop budget. Fits ENGINE_PROTOCOL §2 ("the UI may request one"); no spec edit needed.
- **One owner Pi session (Keel, owner-time call):** P0F (`DEMO_CLASS.md` §5b, `play` only) is folded into Test Session 1 (§6), run first in the same sitting so engine and app failures stay separable. Compass: this replaces the separate P0F session in "Now". Tick 0F from that result.

## Test Session 1 is ready when
#247, #249 and #253 are merged; P1E part 1 is merged; the P1E e2e smoke is green; the Pi runner `ctest -R sidecar_` is green. Keel then posts the exact copy-paste commands on a Phase 1 tracking issue.

## Waiting on the owner
- Nothing until Test Session 1 is ready. (If the owner has 15 minutes on the Pi before then, §5b alone is still useful.)

## Next lead sessions
- Keel: T2-review #247 as soon as it's out of draft (same day); review #250 and #209; confirm #249 merged; merge #253 once reviewed; then post the Test Session 1 commands.
- Compass: Checkpoint A agenda after Test Session 1; apply D13 if #247 stalls; triage the owner's results.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
