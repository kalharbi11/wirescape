<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-13 midday, Keel (New York time).
Board #473's body is live. Only completed exact-head evidence counts. Low-Claude mode is off.

## Priority 1: OSC Out in v1.0.0, Pi first (owner, 2026-11-13; PLAN D21)
- Contract `docs/specs/OSC_OUT.md` (Keel; GPT-family T2 review on the spec PR).
- O1 #730 module + OutBus + config (gpt-builder1): headers commit first, as a draft, today.
- O2 #731 sender + `status.osc.out` + loopback tests + `ci/pi` (gpt-builder3), on O1's headers.
- O3 #732 Studio card (gpt-builder2), FakeEngine only, independent.
- O4 #733 guide + example patch + stdlib receiver (helper lane). O5 #734 Pi soak + owner check (Codex Lead).
- Reviews: Claude agents review the GPT T2 packets (Coda, claude-builder1, claude-reviewer, Keel).

## Priority 2: owner Windows findings
- #722 ASIO list/switch (claude-builder1, T2), #723 failed-driver list (gpt-builder2, after O3),
  #724 MIDI to CV pitch/gate (gpt-builder1, after O1). Evidence steps on #581.

## Priority 3: Qt cleanup (fills waits)
- R3b #713 merged `0f8a674`; `archive/qt-final` on `e6e927c` (owner, pre-release). Done.
- #712 → R5c #716 → #719 (gpt-builder3); R4 #481 → R4b #718 (gpt-builder1).

## 1.0.0
- `cdb74f2` is superseded by D21. Keel names a new candidate after O1–O4 merge and O5 passes;
  the owner tests it (Pi first, then Windows; #581). Annotated `v1.0.0` on that SHA, plus
  `archive/chaos-performance-strip-554` on `73bfe95`. Claude sessions can't push tags (the owner does).

## Parked
R6 #470, Web AudioWorklet (#658 P1.2), #600, #337, #332, #91, #594, OSC Out to LAN.
No devices, LAN, SSH, force/direct-main pushes or weakened tests.

## Codex Lead review gates (2026-11-13)
- #735 rev 2 merged; prior UDP/refusal/default blockers are resolved.
- O1 #736 c823787: hosted workflows green; awaits Claude-family independent T2 review.
- O2 #739 9d9e222: integrate corrected O1; repair eight clang-format diagnostics;
  native protocol fails absent osc.out registration on its inherited O1 base.
- O3 #738 8eb6a33: native CI green; Web sequence-panel.spec.ts:46 playhead assertion red;
  selected acceptance lacks its task on that checkout. Integrate main and diagnose without weakening tests.
- O4 #740 a4216df: CI green; source High on UDP guide claim and timing/duration fixes remain.
- O5 #734 remains dependency blocked; no Pi soak or owner hardware evidence claimed.
- #744 61beb8d: matcher/Save fixes submitted; exact-head CI pending.
- #728 conflicts on STATE; keep main's state while retaining its other authorized changes.
- #716/#729 retain authorization/acceptance gates; #724 needs owner's failing patch.
Signed: Codex Lead, 2026-11-13
