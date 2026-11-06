<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 ~11:30 UTC by Keel (sixth pass). **Owner priority: get the Pi working fully. Windows waits.**

## Where things stand
- **Merged today:** P1B sidecar (#247), Pi appliance (#264), e2e smoke (#265), **2A live edits (#209, `6e92eed`)**. Phase 1 code is complete except 1H (#250/#256) and the 1E benchmark.
- **Test Session 1 is posted: #271** (owner, Pi, pinned `b7001b2`: §5b M8 x 3 patches, then the app checklist). Keel ruling: the contention benchmark doesn't gate the owner's sitting; it gates Phase 1 sign-off.
- Closed done issues #260, #233, #231, #227.
- Known non-required red: `cortex-m7` / portability probe (#159). `static` is informational.

## Open lanes (prompts were given to the owner on 2026-11-06; each names its issue)
1. **#258** null-sink render histogram + `--bench-summary` (T2, s). Then the wirescape-pi runner contention benchmark (P1E `pi:` line 2).
2. **#267** 2A-2: wire live replan into the sidecar (T2, m). Keel's two #209 Mediums are in scope.
3. **#256/#250** 1H: native sidecar side of the golden transcript (T2). Must be green before 1E signs off.
4. **#266** 1B-2: Windows raw-handle transport (T2); ruling is in the issue. After Test Session 1.
5. **#263/#259** modules: apply Keel's `legacy_index` ruling (T1).

## Phase 1 sign-off needs
#271 owner results (A + B), 1H green, Pi contention benchmark (zero overruns, p99 < 50% period, or BLOCKED with reason). Then Compass runs Checkpoint A.

## Compass decisions (2026-11-06 check-in)
- Agreed: P0F is folded into Test Session 1; PLAN "Now" and 0F updated. 0E, 1D, 1G ticked; #250 is plan item **1H**.
- **D13 sidecar contingency:** if #247 is red again after Keel's checklist, split it into 1B-1 (protocol, null sink, fake MIDI, fade: everything CI proves) and 1B-2 (PortAudio + real MIDI devices). No third attempt at the full L PR.
- Electron (D1) reconfirmed. Board: closed #70 (done) and #140 (Qt; semantics → 2C); `phase/2`–`phase/5` labels on parked items (labels 3–5 created).

## Rulings this session
- **Crash restarts:** Electron main is the only automatic restarter; a renderer request never resets the crash-loop budget.
- **#247 slow fixture:** supported rates only (8 kHz floor kept); D5 deadlines scale with the block period, and 150 ms holds at normal blocks.
- **T02 / D3:** imported documents round-trip unchanged; only UI-authored content carries synthesized fields.
- **Test Session 1 vs benchmark:** the owner sitting doesn't wait for the benchmark.
- **#209 scope:** sidecar wiring is #267; it retries a busy MIDI try-lock (bounded) and uses D5 directly when audio isn't rendering.
- **Benchmark source:** null-sink histogram + `--bench-summary` stderr line (#258); no protocol change.

## Waiting on the owner
- **#271 Test Session 1** on the Pi (one sitting). Then post results in #271.

## Next lead sessions
- Keel: triage #271 results (engine faults → issue per failure); T2-review #258, #267, #256 native, #266; merge when green.
- Compass: Checkpoint A agenda after Test Session 1; triage the owner's results.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
