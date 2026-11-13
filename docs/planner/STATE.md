<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12 late night, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts. `usage/low-claude` is on.

## 1.0.0
- Candidate **`cdb74f2`** waits on the owner test (#581). On pass: annotated `v1.0.0` on that SHA,
  plus `archive/chaos-performance-strip-554` on `73bfe95`. On fail: #597 → new SHA from Keel.

## Qt removal #661
- R3a #482 merged `e336fc7`. Its auto-merge was armed and all required checks were green, but it
  didn't fire, so Keel merged it on the approved head `e57fd80`. R3b-pre #707 merged `cec2c69`.
- R3b #713 (gpt-builder2): unblocked. Integrate main → retarget → exact head green → Claude T2 →
  lightweight `archive/qt-final` on its main parent → merge.
- R4 #481 (gpt-builder1) keeps its #469 inventory and budget. Keel ruling: the extra Qt tool paths
  (Pi UI probe, Pi preflight, Windows setup, cloud setup) are **R4b #718** (T1), after #481.
- R5c #716 (gpt-builder3, T0) after R3b; it also drops `legacy/**` and `docs/history/**` from AGENTS.md lists.

## CI
- #712 (#701 gate tier fail-closed, gpt-builder3): two test bugs (High), source OK. `accept 701`
  needs #715's task file on main. #715 (Codex Lead) now conflicts on STATE.md: take main's copy.
- #719 (gpt-builder3, T2, after #712): guard `is_test()` flags `docs/**.md` named "*test*".
  #708 hit it; Keel applied `allow-test-removal` for its four task-file moves only.
- #708 task sweep (Keel, T0): fresh approval on `4f937ca`, guard re-run; gate merges on green.
- #710 OSC fuzz (claude-builder1): waits for low-Claude mode to end, a discoverable #676 task, and a GPT T2.

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594 until the owner's next plan.
No devices, LAN, SSH, force/direct-main pushes or weakened tests.
Signed: Keel, 2026-11-12
