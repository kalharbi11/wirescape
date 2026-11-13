<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-13, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts. Low-Claude mode is **off**
(owner, 2026-11-13): `usage/low-claude` is gone from #473; Claude builders and reviews resume.

## 1.0.0
- Candidate **`cdb74f2`** waits on the owner test (#581). On pass: annotated `v1.0.0` on that SHA,
  plus `archive/chaos-performance-strip-554` on `73bfe95`. On fail: #597 → new SHA from Keel.
- Claude cloud sessions can't push tags (the proxy refuses). The owner or Codex Lead creates the tags.

## Qt removal #661
- R3a #482 `e336fc7`, R3b-pre #707 `cec2c69` merged.
- R3b #713 head `7776515`: Coda Claude-family T2 (no High) + Keel CI-policy approval. The gate merges it.
- `archive/qt-final`: lightweight, on the **first parent of #713's squash commit**, created once
  right after the merge by Codex Lead or the owner; SHA posted on #661. (Amends "just before merge".)
- Then R5c #716 (gpt-builder3, T0) and R4 #481 (gpt-builder1, its #469 inventory only),
  then R4b #718 (gpt-builder1, the Qt tool leftovers).

## CI
- #710 OSC fuzz merged `d4cce43` (non-required, read-only). Keel checked it after the merge: OK. The gate doesn't
  enforce Keel's extra CI-policy approval; reviewers wait for it on required or write-permission workflows.
- #715 merged: #701's task file is on main. #712 (gpt-builder3): new head, CI running → `accept 701`
  → Claude-family T2 + Keel.
- #719 (gpt-builder3, after #712): task file corrected to `tools/ci/test_guard.py`.

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594, #676's Pi nightly part, until
the owner's next plan. No devices, LAN, SSH, force/direct-main pushes or weakened tests.
Signed: Keel, 2026-11-13
