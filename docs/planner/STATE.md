<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12 evening, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts.

## 1.0.0
- Candidate **`cdb74f2`** (#700 versions + #703 PowerShell 5.1 + #706 cables) posted on #581.
  `cdb74f2..9c903ad` is docs-only and `main@9c903ad` CI/CodeQL are green.
- Owner tests that exact SHA (#581). On pass: Codex Lead or owner tags annotated `v1.0.0`
  on `cdb74f2`, plus `archive/chaos-performance-strip-554` on `73bfe95`. On fail: #597 → new SHA.
- #700's 9 files vs ≤8: accepted by Keel (version strings span 9 files; no extra scope).

## Qt removal #661
- R3a #482 `e57fd80` (gpt-builder2): Keel ruled the frozen-renderer default-graph adapter
  acceptable; Claude T2 review has no High. Approve when Windows shows
  `PASS frozen Qt vs Qt-free` and ASan is green.
- R3b-pre #704/#707 (gpt-builder1): size raised to ≤650 lines/≤10 files (formatted tests).
  Needs exact-head green, `accept 704`, Claude T2 review. R3b #468 waits on it.
- Keel tags `archive/qt-final` on the R3b base commit before R3b merges.

## CI
- #693 CI speed 1 (gpt-builder3): Keel CI-policy approval on `622441a`; gate merges.
- #702 board/post-merge watcher (Keel) `8f79c1e`: both Mediums fixed; needs GPT T2
  re-review. Board `followups` is red on PRs until it lands.
- #710 OSC fuzz (claude-builder1): protected-path line added; needs GPT T2, then Keel approval.
- #701 gate fails closed on missing tier (gpt-builder3): not started.

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594 until the owner's next plan.
No devices, LAN, SSH, force/direct-main pushes or weakened tests.
Signed: Keel, 2026-11-12
