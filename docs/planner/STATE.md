<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 by Keel (second pass: took over #247, approved #253, rulings for #256/#257, new tasks #258–#260), after Compass's check-in.

## Where things stand
- **Merged:** P0D, P1A, P1.0, P0E, P1G, P1D (#242), **P1C (#249)**, plus lead docs.
- **#247 P1B sidecar: Keel took it over.** Pushed `19bb2f2`: the 1 Hz fixture was invalid (engine minimum 8 kHz), so it was replaced by a stricter 8 kHz × 4096 case. That exposed a real D5 bug (fixed 100/150 ms swap deadlines → every swap refused once a block period is longer than that), now `max(budget, N periods)`. Also clang-format plus the clang-tidy/cppcheck fixes. Local native ctest 68/68. Keel's T2 review of the GPT part: no High or Medium. Out of draft. **Needs:** green CI on `19bb2f2`, the Pi `sidecar_` run, and a GPT review of `19bb2f2` only; then Keel merges. If Windows `:160` (fake MIDI off) is still red, the engine lane takes that one item. D13 split not needed unless that sticks.
- **#253 P1F shell:** Keel T1 review done, approved `87a2016`, `ready-to-merge`; auto-merge on green.
- **#256 (1H, Studio half):** Keel ruled spec T02 wins. PatchDoc must not add defaults to imported docs; authored content (addModule, new patch, starters) may. Fix goes in #256 (+4 files/+150 lines authorized); now T2.
- **#257 (P1E part 1):** Keel ruled the benchmark reads a new stderr `bench …` line (#258), not `cpuPercent`. Merge part 1 when green; the runner PR follows.
- **#209 (2A):** green on its last head incl. Pi (10.7 ms gap); only static clang-tidy items left (lane 4).
- Known non-required red: `cortex-m7` (#159).

## Compass decisions (2026-11-06 check-in)
- Agreed: P0F is folded into Test Session 1; PLAN "Now" and 0F updated. 0E, 1D, 1G ticked; #250 is plan item **1H**.
- **D13 sidecar contingency:** if #247 is red again after Keel's checklist, split it into 1B-1 (protocol, null sink, fake MIDI, fade: everything CI proves) and 1B-2 (PortAudio + real MIDI devices). No third attempt at the full L PR.
- Electron (D1) reconfirmed. Board: closed #70 (done) and #140 (Qt; semantics → 2C); `phase/2`–`phase/5` labels on parked items (labels 3–5 created).

## Lanes (copy-ready prompts in `docs/planner/LANES.md`)
1. Engine: #247 last items → #258 bench summary → #250 native checker.  2. Desktop: P1E part 2 e2e smoke (after #247).
3. Integration: #256 PatchDoc fix → #257 merge + runner PR.  4. Live edits: #209 static → wire live replan into the sidecar.
5. **New:** modules #259 (Step Sequencer + Random Source into the catalogue).  6. **New:** Pi appliance #260 (after #253).

## Rulings this session
- **Crash restarts:** Electron main is the only automatic restarter; a renderer request never resets the crash-loop budget.
- **#247 slow fixture:** supported rates only (8 kHz floor kept); D5 deadlines scale with the block period, and 150 ms holds at normal blocks.
- **T02 / D3:** imported documents round-trip unchanged; only UI-authored content carries synthesized fields.
- **Benchmark source:** null-sink histogram + `--bench-summary` stderr line (#258); no protocol change.

## Test Session 1 is ready when
#247, #249 and #253 are merged; P1E part 1 is merged; the P1E e2e smoke is green; the Pi runner `ctest -R sidecar_` is green. Keel then posts the exact copy-paste commands on a Phase 1 tracking issue.

## Waiting on the owner
- Nothing until Test Session 1 is ready. (If the owner has 15 minutes on the Pi before then, §5b alone is still useful.)

## Next lead sessions
- Keel: merge #247 once CI, the Pi run and the GPT review of `19bb2f2` are in; T2-review #256, #258 and #209; then post the Test Session 1 commands.
- Compass: Checkpoint A agenda after Test Session 1; apply D13 if #247 stalls; triage the owner's results.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
