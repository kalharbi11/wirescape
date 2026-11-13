<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-13, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts.

## Priority 1: owner Windows test fixes (2026-11-13)
- #724 MIDI to CV pitch/gate (gpt-builder1, T1): draft #727.
- #722 ASIO list/switch (claude-builder1, T2): not started.
- #723 Studio failed-driver list: merged #726 `f25223a` (Coda T1).
- Owner evidence and steps: #581, triage #597.

## 1.0.0
- Candidate **`cdb74f2`** waits on the owner test (#581). On pass: annotated `v1.0.0` on that SHA,
  plus `archive/chaos-performance-strip-554` on `73bfe95`. On fail: fixes via #597 → new SHA from Keel.
- Claude cloud sessions can't push tags; the owner or Codex Lead creates them.

## Qt removal #661
- Merged: R3a #482 `e336fc7`, R3b-pre #707 `cec2c69`, R3b #713 `0f8a674`, R4 #481 `9ad4260`.
- `archive/qt-final` exists (lightweight) on `e6e927c`, R3b's first parent.
- Next: R5c #716 (gpt-builder3, T0; unblocked now that R3b merged), R4b #718 (gpt-builder1, Qt tool leftovers).

## CI
- #712 (#701 gate fails closed on a missing tier, gpt-builder3): Coda cross-family + Keel CI-policy approved
  `d6768bd`; the gate merges it on green. Then #719 (guard `is_test` docs false positive, gpt-builder3).
- #710 OSC fuzz workflow merged `d4cce43` (non-required).
- Post-merge watcher (#702) dispatches `main_verify` per main commit.

## Pi
- #296 60-min Pi edit soak (gpt-builder2): waits on a Claude-family T2 review of its private runner PR.

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594, #676's Pi nightly part, until
the owner's next plan. No devices, LAN, SSH, force/direct-main pushes or weakened tests.
Signed: Keel, 2026-11-13
