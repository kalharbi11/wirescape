<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 ~11:00 UTC by Keel (fifth pass). **Owner priority: get the Pi working fully. Windows waits.**

## Where things stand
- **Merged:** P0D, P1A, P1.0, P0E, P1G, P1D, P1C (#249), P1F shell (#253), P1E part 1 (#257), **P1B sidecar (#247, `5756f4f`)**: hosted green on `9390fa8`, Pi `pi5` full CTest green on `473d52e` (last commit is a const-ref only).
- **#265 e2e smoke:** out of draft; main merged in; `ce8dd8c` builds the sidecar from the checkout (the #247 fetch is removed). Keel approved; auto-merges on green.
- **#264 Pi appliance:** Windows red was the Linux-only XDG installer test; it's now skipped on `nt` only (Keel ruling, platform scope). Main merged in; approved `5715004`; auto-merges on green.
- **#209 (2A live edits):** main merged in as `7076cc5` (docs conflict only; `app_engine.cpp` auto-merged with #247's D5 fade, so CI is the real check). Merge when hosted (incl. TSan) and `pi5` are green on that head. Then #267 (2A-2).
- **#266 (1B-2 Windows stall):** Keel ruling posted: replace the iostream transport with raw handles (`ReadFile`/`WriteFile`, `read`/`write`), chunked reads, drop the win32 skip. T2.
- **#263 modules:** red, waiting for a builder to apply Keel's `legacy_index` ruling. **#256 (1H, T2):** Studio half done; native checker can start now (#247 merged).
- Known non-required red: `cortex-m7` / portability probe (#159). `static` is informational.

## Pi-first order (critical path to Test Session 1)
1. ~~#247 merge~~ done. 2. #265 green and merged. 3. Private PR15 bench gate on the Pi runner. 4. Keel posts the Test Session 1 commands on a Phase 1 tracking issue (DEMO_CLASS §5b, then the app §6 per the P1E guides).
In parallel: #264, #263, #209 then #267, #256. Windows (#266) after Test Session 1.

## Compass decisions (2026-11-06 check-in)
- Agreed: P0F is folded into Test Session 1; PLAN "Now" and 0F updated. 0E, 1D, 1G ticked; #250 is plan item **1H**.
- **D13 sidecar contingency:** if #247 is red again after Keel's checklist, split it into 1B-1 (protocol, null sink, fake MIDI, fade: everything CI proves) and 1B-2 (PortAudio + real MIDI devices). No third attempt at the full L PR.
- Electron (D1) reconfirmed. Board: closed #70 (done) and #140 (Qt; semantics → 2C); `phase/2`–`phase/5` labels on parked items (labels 3–5 created).

## Lanes (prompts in `docs/planner/LANES.md`; the #209/#267 prompt is in the #209 thread)
1. Engine: #247, #258 bench summary, #250 checker.  2. Desktop: #265 e2e.  3. Integration: #256.
4. Live edits: #209 then #267.  5. Modules: #263.  6. Pi appliance: #264.  Later: #266 Windows stall.

## Rulings this session
- **Crash restarts:** Electron main is the only automatic restarter; a renderer request never resets the crash-loop budget.
- **#247 slow fixture:** supported rates only (8 kHz floor kept); D5 deadlines scale with the block period, and 150 ms holds at normal blocks.
- **T02 / D3:** imported documents round-trip unchanged; only UI-authored content carries synthesized fields.
- **#209 scope:** sidecar wiring is #267; it retries a busy MIDI try-lock (bounded) and uses D5 directly when audio isn't rendering.
- **Benchmark source:** null-sink histogram + `--bench-summary` stderr line (#258); no protocol change.

## Test Session 1 is ready when
Done: #247, #249, #253, P1E part 1, Pi `sidecar_` (on `473d52e`). Left: #265 e2e smoke merged. Keel then posts the exact copy-paste commands on a Phase 1 tracking issue.

## Waiting on the owner
- Nothing until Test Session 1 is ready. (If the owner has 15 minutes on the Pi before then, §5b alone is still useful.)

## Next lead sessions
- Keel: confirm #265 and #264 merged (fix if red); merge #209 when `7076cc5`+ is green incl. TSan/pi5; T2-review #256, #258, #267, #266; post Test Session 1 commands once #265 is in.
- Compass: Checkpoint A agenda after Test Session 1; triage the owner's results.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
