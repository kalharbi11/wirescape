<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12, Coda (senior run). Board #473 BODY is live; agent/<name> owns.
Only completed exact-head acceptance/checks count; queued or skipped is not proof.

## Priority and authority
- Owner order: OSC → Quad VCA/LPG → ASIO, then broad Qt retirement (#661).
- v1.0.0 = #597 fixes + OSC + Quad + ASIO + #599; #593 tags after owner #581.
- Seniors approve T0/T1; T2 needs the other family; Keel approves ABI/spec/CI.
- Claude-family T2 reviews of GPT work are the bottleneck: Coda, Keel or
  claude-builder1 take #659/#648/#631 approvals the moment each head is green.

## Merged this run
- #670 (14d0db1): Windows-portable foundation tests. Its 1e-6 s soak runs
  still looped on Windows' 15.6 ms clock (1 vs 3 cycles); #677 fixes the test.

## OSC first
- #657/#659 (gpt-builder1): Coda pushed format-only 57d84fd (token-identical,
  changed-range clang-format 440 → 0). Coda T2: no High/Medium; Keel spec
  approval carries over. Remaining: green CI + accept657 on 57d84fd → approve.
- #672 (gpt-builder1) starts the moment #659 merges; then close #72.
- #94/#674 (gpt-builder2): acceptance made runnable by Coda (cmake fuzz build,
  hex seed decode, 60 s per target). Coda's local run at b10c244: codec 32.2M,
  pattern 1.3M executions, 0 findings. CI job + Pi nightly split to #676
  (Keel CI-policy ruling). Builder: merge main, run accept 94, T1 review.

## Quad VCA/LPG and ASIO
- Q1 #611 merged (62fa9ec). Q2 #602/#675 (gpt-builder2) in progress, T1.
- #627/#631 (gpt-builder2) 17abf2f: both Codex Highs fixed (OSC/MIDI joined
  before preferred-size swap, rebound after; UTF-8-safe bounded errors).
  Coda T2: one Medium (status test injects "\\xc3" text, not bytes). Then
  CI + accept627, Claude approval, Codex delta on Claude retry, Keel spec.
- #610: picker (item 1) and ASIO docs (item 3) already on main with tests;
  item 2 is #627. Close #610 when #631 merges.
- #605/#608 merged (3a3bd34).

## Other continuations
- #487/#648 (gpt-builder2) b1403b5: Coda T2 no High/Medium. Static was
  cancelled → rerun triggered; private ci/pi device-off run must be linked.
- Gate merges (GITHUB_TOKEN) get no follow-ups: no push CI, no `Closes #N`
  (#664, #636 stayed open) and no schedules (board + main_verify: 0 runs).
  Keel fix: board.yml `followups` job closes linked issues and dispatches
  main_verify once per main commit, on agents' own events. Real fix = a
  GitHub App token for the gate (owner decision on #473).
- #614/#619 (gpt-builder1) Groove external clock; #629 WASM parity after OSC.
- #673 (Compass) D20 / PLAN Now: integrate main after #670, then fast path.
- #560 CPU table and full native CMake need dependency downloads: cloud
  sandboxes get HTTP 403 from codeload.github.com; use CI or the owner's box.

## Deferred and permanent boundaries
- #482→#468/archive→#481/#469 deferred; #661 keeps replacement/tag gates.
- #600 Stereo Output, #337, #436, #332 wait for the owner's next-version pick.
- #271 hardware, #274 display, #296 3600 s soak, #581/#593 release distinct.
- No weakened tests, main/force push, device/SSH/LAN access.
Signed: Keel, 2026-11-12
