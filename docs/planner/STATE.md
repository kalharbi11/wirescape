<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-12, Codex Lead, owner priority update.
Board #473 is the live dependency-ordered queue. Preserve existing writers.
Required checks and acceptance apply to the final head; queued/stale is not pass.

## 1. OSC first
- C1 #603/#618 and C2 #604/#620 are merged; do not duplicate them.
- Row enable #612/#645: worker source High resolved; remaining proof is
  disabled/enabled/unmatched/mixed-bundle messages → Monitor telemetry.
  Integrate current main and run actual accept612; task parser fixed at9b77d12.
  Authorized850lines/22files; Codex Lead cross-family T2, Keel protected spec.
- Unknown config keys #657/#659 is canonical; duplicate #660 closed unmerged,
  branch/evidence preserved. Ignore unknowns, retain strict known-value checks.
  Integrate known enabled from #645; fix changed-range format, actual accept657/
  Windows/full native+Web, then Coda T2. Do not merge two adapters/task titles.
- Combined OSC closeout: status, presets/Clear, save/load, row outputs/Monitor,
  patch-guide wiring; #94 follows full J5 acceptance.

## 2. Quad VCA/LPG
- #601/#611 Q1 retains writer: permanent slots0..8 and regression,4ch×8voice
  cost, exception boundary, changed-range formatting, conformance/catalogue.
  Keel20files/900lines authorized; actual accept601/required and independent T1.
- #602 Q2 starts only after Q1 merges: Studio card/two patches/docs/tests,
  separate T1 with save/reload/accessibility and Playwright.

## 3. ASIO and its setup
- #627/#631: preferred1..4096, quiescent prepare, one retry, actual size;
  no callback resize/persistence; preserve attempted host error through rollback.
  Integrate main/task, fake positive/8192/retry-fail, Windows, Coda/spec review.
- #605/#608: exact pinned Ninja suffix ruling from Keel, host-architecture/
  global-pin recovery fixes and real Windows CheckOnly/W1/launcher smoke.
  Broad version ranges deferred. Owner alone verifies physical ASIO sound.

## Supporting work and completed review
- Qt analyzer #653/#654 MERGED; verify current-head effect, no deletion needed.
- #617b06e1bb MERGED after final Codex review5868834370 and Keel disposition;
  no remaining Codex review or CI-policy approval request on that PR.
- #6409d415b5 Windows Git-Bash/LF/fake-gh harness: four local cases pass;
  actual hosted required checks and Keel refreshed policy approval still govern.
- #6470b1d428 bounded Web macro refusal/static repair: actual acceptance/
  real-WASM/Static evidence exists; final required checks and independent T1.
- Existing Groove #619/#626, Studio #607, D5 #648 retain writers, below OSC
  for unrelated review/lead capacity. No #526/#528 changes in this lead session.

## 4. Broad Qt retirement deferred
- #482 R3a → #468 R3b/archive → #481/#469 R4; #470 R6 is Keel-led.
  Resume only after OSC/VCA/ASIO; preserve drafts, frozen tests and tag gates.
  A demonstrated direct Qt blocker gets only the smallest targeted repair.
- #271/#274 hardware, #296 actual3600s, #581/#593 release remain separate.
- No new feature starts, weakened checks, direct main/force push or devices/SSH.
