<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. Rewrite it at the end of each lead session; history lives in git.

**Updated:** 2026-11-06 ~10:00 UTC by Keel (third pass). **Owner priority: get the Pi working fully. Windows waits.**

## Where things stand
- **Merged:** P0D, P1A, P1.0, P0E, P1G, P1D, P1C (#249), **P1F shell (#253)**. #257 (P1E part 1) is auto-merging on green.
- **#247 P1B sidecar: D13 applied as "Pi/Linux first".** Windows protocol test skips (exit 77) and moves to **#266 (1B-2)**. Head `46d5464`: windows, linux-arm64, acceptance green; sanitizers running. **Merge when:** sanitizers green + Pi `sidecar_` run posted on the PR + a GPT COMMENT review of Keel's delta `19bb2f2..46d5464` (~60 lines). Any lead merges then.
- **#209 (2A live edits):** Keel T2 review posted, no High (two Mediums for the integration). Green incl. TSan and Pi (10.68 ms). Claim released; a ChatGPT agent takes it. After #247 merges: merge `main` in, re-green, Keel merges. **Sidecar integration is a separate issue, #267 (2A-2)**; #209 is at its size budget.
- **#264 Pi appliance (#260), #265 P1E e2e smoke, #263 modules (#259, unblocked by Keel), #256 (1H, T2):** drafts in progress.
- Known non-required red: `cortex-m7` / portability probe (#159). `static` is informational.

## Pi-first order (critical path to Test Session 1)
1. #247 merge, 2. #265 e2e smoke green (sidecar x Electron, null sink), 3. private PR15 bench gate on the Pi runner, 4. Keel posts Test Session 1 commands.
In parallel: #264 autostart, #263 modules, #209 then #267, #256. Windows (#266) only after Test Session 1.

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
#247, #249 and #253 are merged; P1E part 1 is merged; the P1E e2e smoke is green; the Pi runner `ctest -R sidecar_` is green. Keel then posts the exact copy-paste commands on a Phase 1 tracking issue.

## Waiting on the owner
- Nothing until Test Session 1 is ready. (If the owner has 15 minutes on the Pi before then, §5b alone is still useful.)

## Next lead sessions
- Keel: merge #247 when its three items are in; merge #209 once it's up to date and green; T2-review #256, #258, #267; post Test Session 1 commands.
- Compass: Checkpoint A agenda after Test Session 1; triage the owner's results.

## Standing rules from the owner
- Don't plan agent assignment; the owner and their managers assign. Keep sections parallel where possible.
- Pi first (Windows allowed for faster smoke tests); touchscreen, Plaits polish and a CHAOS custom panel go to Phase 5.
- No browser/kiosk stage: the product is a standalone app window with the web UI inside.
- Real-device tests are the owner's; the Pi runner and the laptop run offline tests only.
- Checkpoints are spread out and don't stop work.
- Leads save usage: delegate mechanics (Codex or a subagent), don't poll, ask the owner only for approvals and hardware checks.
