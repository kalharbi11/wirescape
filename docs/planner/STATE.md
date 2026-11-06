<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 by Codex Lead (independent build queue; Keel rulings retained). **Owner priority: get the Pi working fully. Windows waits.**

## Where things stand
- **Merged today:** P1B sidecar (#247), Pi appliance (#264), e2e smoke (#265), **2A live edits (#209, `6e92eed`)**. Phase 1 code is complete except 1H (#250/#256) and the 1E benchmark.
- **Test Session 1 = #271**, expanded (owner can't test yet): builds `origin/main`; ★ A M8 patches, B app (+save round trip), C robustness (engine kill, M8/audio unplug, panic), D1 chrt/top; ☆ D2 block 128/64, D3 owner-run contention benchmark (Phase 1 perf gate; needs #274), D4 30-min MIDI soak, E boot-to-sound (#264), F seamless edits (needs #273), G new modules (needs #263). **Keel keeps the PR table at the top of #271 current.**
- Closed done issues #260, #233, #231, #227.
- Known non-required red: `cortex-m7` / portability probe (#159). `static` is informational.

## Open lanes (existing builders retained; new briefs await owner dispatch)
1. **#274** (#258 bench summary): Keel T2 done, no High; needs the line format in `native/sidecar/DESIGN.md`, green + `pi5`, then Keel merges.
2. **#273** (#267 live replan in sidecar): Keel T2 done, no High; Medium = stderr `commit path=live|d5 gen=N` + process-test asserts. Then merge (unlocks #271 F).
3. **#256** (#250 1H): Keel ruled the 4 fixture/engine conflicts (category is an engine bug; fake M8, null-device status are engine-correct; T11/T13 fixture fixes, protected-path line on #250); budget 11 files.
4. **#263** (#259 modules): merged at `305d660`; Step Sequencer + Random Source available.
5. **#266** 1B-2 Windows raw-handle transport: not started; after Test Session 1.
6. **#276 / #279** patch-folder defaults: implemented, draft; Studio build + desktop TS + local acceptance PASS; Electron CI and independent T1 review pending.
7. **#277** clean-switch/immediate-reload autosave fix; **#278** fake-device edit-soak harness: exact independent build briefs ready, unclaimed. Owner requested Phase 2D/2F work during test deferral; goals unchanged.

## Phase 1 sign-off needs
#271 owner results (A, B, C), 1H green, contention benchmark (#271 D3 or the Pi runner; zero overruns, p99 < 50% period). Then Compass runs Checkpoint A.

## Compass decisions (2026-11-06 check-in)
- Agreed: P0F is folded into Test Session 1; PLAN "Now" and 0F updated. 0E, 1D, 1G ticked; #250 is plan item **1H**.
- **D13 sidecar contingency:** if #247 is red again after Keel's checklist, split it into 1B-1 (protocol, null sink, fake MIDI, fade: everything CI proves) and 1B-2 (PortAudio + real MIDI devices). No third attempt at the full L PR.
- Electron (D1) reconfirmed. Board: closed #70 (done) and #140 (Qt; semantics → 2C); `phase/2`–`phase/5` labels on parked items (labels 3–5 created).

## Rulings this session
- **Crash restarts:** Electron main is the only automatic restarter; a renderer request never resets the crash-loop budget.
- **#247 slow fixture:** supported rates only (8 kHz floor kept); D5 deadlines scale with the block period, and 150 ms holds at normal blocks.
- **T02 / D3:** imported documents round-trip unchanged; only UI-authored content carries synthesized fields.
- **Test Session 1 vs benchmark:** the owner sitting doesn't wait for the benchmark; the owner's D3 run counts as the contention evidence (runner has no display).
- **#256 golden transcript:** illustrative device values; checker asserts shapes/order/gen/§7 invariants, literal only for session-independent values.
- **#209 scope:** sidecar wiring is #267; it retries a busy MIDI try-lock (bounded) and uses D5 directly when audio isn't rendering.
- **Benchmark source:** null-sink histogram + `--bench-summary` stderr line (#258); no protocol change.

## Waiting on the owner
- **#271 Test Session 1** deferred by owner; keep building. Conditional H (patch folder, #276) and I (session recovery, #277) are in a signed comment; skip until merged. No hardware action requested now.

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
