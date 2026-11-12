<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12 night, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts.

## 1.0.0
- Candidate **`cdb74f2`** waits on the owner test (#581). On pass: annotated `v1.0.0` on that SHA,
  plus `archive/chaos-performance-strip-554` on `73bfe95`. On fail: #597 → new SHA from Keel.

## Qt removal #661
- R3a #482 `e57fd80`: Keel T2 approval (Windows frozen Qt vs Qt-free equality PASS). Ready; gate merges.
- R3b-pre #707 `6117abe`: Keel T2 approval (580/650 lines). Ready; gate merges.
- R3b #713 draft (gpt-builder2): Keel ruled the two Qt icon deletions an accepted acceptance
  deviation; `allow-protected` + `allow-test-removal` applied (Qt-only tests in its table).
  Next: integrate main after #482/#707 → green → Claude T2 → `archive/qt-final` on its main parent → merge.
- R5c #716 draft (gpt-builder3, T0): blocked on R3b; must also drop `legacy/**`, `docs/history/**`
  from AGENTS.md lists (and guard/CODEOWNERS if named). R4 #481 after R3b.

## CI
- #702 merged `7426ea4`; post-merge watcher dispatched `main_verify` for it (success).
- #693 CI speed 1 merged. #712 (#701 gate tier fail-closed): two test bugs (High), source OK;
  `allow-protected` applied. Needs #715's task file for `accept 701`.
- #710 OSC fuzz (claude-builder1): waits on GPT T2 review and a discoverable #676 task title.
- #708 task sweep (Keel, T0): main merged in, PLAN hunk dropped; gate merges on green.

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594 until the owner's next plan.
No devices, LAN, SSH, force/direct-main pushes or weakened tests.
Signed: Keel, 2026-11-12
