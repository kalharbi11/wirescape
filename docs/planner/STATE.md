<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-13 afternoon, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts. Low-Claude mode is off.

## Priority 1: OSC Out in v1.0.0, Pi first (owner, 2026-11-13; PLAN D21)
- Contract `docs/specs/OSC_OUT.md` rev 2 merged (#735, Codex Lead T2).
- **Merged:** O1 module #736 (Keel T2) and O2 sender + `status.osc.out` #739 (Keel T2 + spec).
- Open: O3 Studio card #738 (gpt-builder2; rebase, drop provisional FakeEngine entry), O4 guide +
  receiver #740 (claude-helper; Codex Lead fixes + choke `key -1` + re-send after edits), then O5 Pi
  soak + owner Pi check #734 (Codex Lead). Then Keel names the new 1.0.0 candidate.
- Follow-ups: O1 phantom gate rise after reset (gpt-builder1, Medium); #747 acceptance-job Chromium (gpt-builder2).

## Priority 2: owner Windows findings
- #722 ASIO list/switch (claude-builder1, T2), #723 failed-driver list (gpt-builder2, after O3),
  #724 MIDI to CV pitch/gate (gpt-builder1, after O1). Evidence steps on #581.

## Priority 3: Qt cleanup (fills waits)
- R3b #713 merged `0f8a674`; `archive/qt-final` on `e6e927c` (owner, pre-release). Done.
- #712, #719, R4 #481, R4b #729 merged. R5c #716 (gpt-builder3) remains.

## 1.0.0
- `cdb74f2` is superseded by D21. Keel names a new candidate after O1–O4 merge and O5 passes;
  the owner tests it (Pi first, then Windows; #581). Annotated `v1.0.0` on that SHA, plus
  `archive/chaos-performance-strip-554` on `73bfe95`. Claude sessions can't push tags (the owner does).

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594, OSC Out to LAN.
No devices, LAN, SSH, force/direct-main pushes or weakened tests.
Signed: Keel, 2026-11-13
