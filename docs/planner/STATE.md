<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12, Codex Lead, v4.1 senior run.
Board #473 BODY is live; agent/<name> owns work; edit Current state in place.
Owner order: OSC → Quad VCA/LPG → ASIO; preserve existing writers.
Completed exact-head acceptance governs; queued/running/skipped is not a pass.

## OSC first
- C1 #618, Monitor #620 and row enable #645 are merged.
- #612 closed after confirming #645 merge df656cf (source f73867c).
- #657/#659 c55552a: gpt-builder1; Web43199301180 fails TS2345 at
  apps/studio/tests/osc-editor.spec.ts:115,119 (object vs config record/array).
  Precisely type fixtures; preserve assertions. CI/Static still running.
  Authored config.version absent/1 only; enabled is known v1; preserve unknowns.
  After repair: accept657/native/Windows/Web, Claude T2 and Keel spec approval.
- #672: gpt-builder1, blocked only on #659 now. Combined loopback/row/Monitor,
  save/reload/presets/Clear and CONTROLLERS.md; then close #72, unblock #94.

## Quad VCA/LPG
- #601/#611 fa7db47: gpt-builder2 applied Keel's exact private: format fix.
  Earlier b743949 CI/accept601/Web passed; new CI/Static/Web still running.
  Prior independent source review has no High; green final head still required.
  Preserve slots0..8, cost row, exception boundary; budget20files/900lines.
- #602: same owner, blocked on actual Q1 merge; stale format-stop text removed.
  Q2 remains separate Studio card/patches/guide, Playwright/save-reload and T1.

## ASIO and setup
- #627/#631 2337e6c: claude-builder1 confirmed by Keel; all commits retained.
  CI/Web pass but no Windows proof on this head; ci/windows now applied.
  Keel requests spec sentence relocation and exact audio_recovery test format.
  Fresh head needs Windows/Static, Codex T2 plus Claude review of GPT portions;
  Keel retains spec approval. Only owner checks physical ASIO sound.
- #605/#608 7e3f669: CI43197960801 and selected accept605/Windows smoke pass.
  Codex T1 Medium: setup.ps1:315 real launcher lacks scratch --profile-dir,
  opens normal profile. gpt-builder2 repairs with isolation regression,
  then fresh required/Windows acceptance and final review. Narrow Ninja only.

## Supporting decisions and handoffs
- #487/#648 56918a6 assigned to gpt-builder2 after priority continuations.
  Keel found no High/Medium; separate render_at lambda, no NOLINT, normal sync.
  CI passes; Static red; source-matched private device-off Pi evidence required.
- #614/#619 5a0c261: gpt-builder1; CI/Web pass, Static red; frozen unpatched
  stream and exact external-clock note-offs remain review obligations.
- #636/#640 merged3aca7f6; no actual main_verify evidence retrieved this run.
  gpt-builder2 links real main SHA/run before closure; remove stale old block.
- #666 d7b754f: prior Codex Medium on skipping Windows foundation remains.
  Keel rules on coverage; green code-path CI is not docs-only regression proof.
- #663 conflicts; Compass integrates priorities, #665 D20 and #670 STATE
  without losing this snapshot. #664 task discoverability still required.

## Deferred and permanent
- Broad #482→#468/archive→#481/#469 deferred behind OSC/VCA/ASIO; #661's
  parallel wording needs Keel alignment. Keep all replacement/tag/review gates.
- #271/#274 hardware, #296 actual3600s, #581/#593 release remain separate.
- No new features, weakened gates, main/force push, devices, LAN or SSH.
