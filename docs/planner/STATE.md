<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-12, Codex Lead, owner priority update.
Board #473 is the live dependency-ordered queue. Preserve existing writers.
Required checks and acceptance apply to the final head; queued/stale is not pass.

## 1. OSC first
- C1 #603/#618 and C2 #604/#620 are merged; do not duplicate them.
- Row enable #612/#645: worker source High resolved; remaining proof is
  Monitor/mixed-bundle regression17bcf9f and main7fd9948 integration ab92f77
  pushed; both Monitor and row suites retained. Actual accept612/CI pending.
  Authorized850lines/22files; Codex Lead cross-family T2, Keel protected spec.
- Unknown config keys #657/#659 is canonical; duplicate #660 closed unmerged,
  branch/evidence preserved. Ignore unknowns, retain strict known-value checks.
  b62beb6 removes duplicate known-row diagnostics via parser fall-through,
  after Keel80034f3 formatting. Integrate known enabled from #645; actual
  accept657/Windows/native+Web and Coda T2 pending. One adapter/task only.
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
  hosted Windows exit90 persists; no flake/green assumed. Keel review needed.
- #647 MERGED as7fd9948; bounded Web macro refusal/rollback fix complete.
- #666 d7b754f: Codex Medium on dropping Windows foundation coverage;
  prefer explicit UTF-8 IO and deterministic test clock, keep assertions.
- #665 needs discoverable #664 task; Compass D20 says these finish v1.0.
- Lead automation aligned to current owner hierarchy; schedule unchanged.
- Existing Groove #619/#626, Studio #607, D5 #648 retain writers, below OSC
  for unrelated review/lead capacity. No #526/#528 changes in this lead session.

## 4. Broad Qt retirement deferred
- #482 R3a → #468 R3b/archive → #481/#469 R4; #470 R6 is Keel-led.
  Resume only after OSC/VCA/ASIO; preserve drafts, frozen tests and tag gates.
  A demonstrated direct Qt blocker gets only the smallest targeted repair.
- #271/#274 hardware, #296 actual3600s, #581/#593 release remain separate.
- No new feature starts, weakened checks, direct main/force push or devices/SSH.
