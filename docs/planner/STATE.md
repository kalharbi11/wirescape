<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12, Codex Lead, full senior run.
Board #473 BODY is live; current owner order supersedes stale PLAN/main snapshots.
Final-head acceptance and required checks govern; no stale/queued result passes.

## OSC first
- C1 #618 and Monitor #620 merged; preserve both suites.
- #612/#645 ab92f77: designated Codex T2 approval posted; ready for merge.
  Guard, Linux, Windows, sanitizers, selected accept612, Static/Web/site success.
  Keel spec approval6818411925: #645 FIRST. Auto-review rejected direct merge;
  owner explicit execution approval requested; no bypass or merge claimed.
- #657/#659 b62beb6: builder-chat-1 continues AFTER #645 lands, merging main.
  Keel ruling 6818391090: JSON config.version absent/1 accepted, all else refused;
  enabled known/validated, parser-owned unknown diagnostics, spec/README updates.
  Budget 10 files/320 lines; one-function format-only commit separately excluded.
  CI/Web succeeded; Static fails 428 header-format findings. Coda final T2 needed.
- Combined closeout: save/load, presets/Clear, row outputs/Monitor, truthful status,
  guide wiring. #94 waits for full J5; no duplicate OSC implementation.

## Quad VCA/LPG
- #601/#611 1a2c8bd: builder-chat-2; normal main integration and Q1 repairs.
  Codex independent T1 source review: no High; exact CI/Web/Static still running.
  Slots0..8, four-channel/eight-voice cost row and exception boundary present.
  Authorized20files/900lines. No measured pass inferred from source review.
- Q2 #602 only AFTER native Q1 merge; separate Studio/patch/docs/tests T1.

## ASIO and setup
- #627/#631 7920601: authorized GPT builder-chat-3 takeover; labels corrected.
  CI/Web fail constructor mismatch at portaudio_service.cpp:279, not a flake.
  Reconcile declaration/definition/fakes; preserve quiescent1..4096/one retry,
  actual buffer reporting and attempted host error. Coda T2/Keel spec required.
- #605/#608 3f8040d: Keel now accepts ONLY pinned1.13.0 Kitware suffix via
  exact regex on #608. Builder-chat-2 applies positive/negative tests, x64-host
  and global-Ninja recovery fixes; actual Windows CheckOnly/W1/launcher proof.
  Broad toolchain ranges deferred; owner alone checks physical ASIO sound.

## Supporting decisions and handoffs
- #640 9d415b5: Windows fake-gh test exit90 is real, not classified as flake.
  Existing branch conflicts: integrate main and repair harness; Keel cost ruling.
- #666 d7b754f: prior Codex Medium on skipping Windows foundation still open.
  Prefer explicit UTF-8 IO/deterministic test clock; preserve assertions/coverage.
- #668 b6032bd: Keel High—revoke auto-merge on ANY failed decision and pin
  enable with --match-head-commit; builder-chat-2 repairs, Keel final review.
- #607 bc8bed3: all workflows now green; cache fixed. Keel accepts UI scope
  BUT requires cabled-only FakeEngine keys, cabled success/no-cable unavailable
  tests before approval; builder-chat-1 implements inside500lines/8files.
- #619 3a4608c: CI/Web success, Static red; retain existing Groove branch,
  frozen unpatched stream and exact note-off evidence obligations.
- #648 56918a6: Coda T2 phase-order review pending; CI success, Static red;
  source-matched private Pi/Windows evidence remains mandatory.
- #629 parent stays open after merged subsets; visual parity follows #607.
- #663/#665 conflict with main. Compass integrates PLAN priorities + D20 and
  discoverable #664 task; these lanes finish v1.0, no speculative v1.1.
- Main push CI last4539713 (42992470721), current main5de0a92 unverified.

## Deferred and permanent
- #482 R3a → #468 R3b/archive → #481/#469 R4 deferred behind OSC/VCA/ASIO;
  #470 Keel-led. Preserve replacement tests/tags/drafts; targeted blockers only.
- #271/#274 hardware, #296 actual3600s and #581/#593 release are separate.
- No new features, weakened checks, main/force push, devices or SSH.
