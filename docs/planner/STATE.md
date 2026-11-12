<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-12 ~10:30 UTC, Keel (second run). Owner failures preempt features.
Board #473 BODY is the live list; agent/<name> owns an item (AGENTS.md v4, LEADS.md roster).
Completed source-head evidence governs; running, missing and stale results never pass.

## Priority and authority
- Owner order (#473, 2026-11-12): OSC → Quad VCA/LPG → ASIO; broad Qt cleanup after them (#661 re-sequenced).
- 1.0: #599/#607 is the last quick fix. Then the #593 candidate PR (all versions 1.0.0),
  then the owner test #581 on that merged SHA, then tag v1.0.0 on exactly that SHA (Keel ruling on #593).
- Seniors approve T0/T1; opposite-family T2 is mandatory; Keel approves ABI/spec/CI policy.
- Signer identity is an auditable convention, not enforced (Keel, #658 P0.2).

## Keel rulings this run (2026-11-12)
- #605/#608: accept Ninja `^1\.13\.0(\.git\.kitware\.jobserver-pipe-\d+)?$` only; listed
  positive/negative tests; real Windows -CheckOnly + launcher smoke on the exact head.
- #599/#607: connected-only Random history accepted (ENGINE_PROTOCOL.md:152). FakeEngine
  must emit `outputs` only for cabled outputs; uncabled/sidecar history → Compass, v1.1.
- OSC #645 ↔ #659: #645 lands FIRST, no version bump (main refuses unknown keys today).
  #659 then adds the authored `config.version` gate (absent/1 ok, else refuse), unknown
  keys ignored + diagnostics, and "restricting keys bump version". Budget 10 files/320 lines;
  a format-only commit is allowed for `runtime_module_config_impl`. #645 spec: Keel-approved.
- #668 (merge gate): High. A comment run cancels the pending `synchronize` run, so every "no"
  decision must `--disable-auto`, and the enable uses `--match-head-commit`. #667 revised.
- #640: Web dispatch only on path match (parsed from workflows); re-dispatch once on
  `cancelled`; 170/4 budget. Windows `90 != 0` is real; branch conflicts.
- #648 D5: Keel T2 no High/Medium. Low: Static tidy at protocol_unit_tests.cpp:395;
  then merge main, link the Pi run; any Claude agent approves.

## Keel run 2 (10:10–10:30 UTC)
- INCIDENT: #640 merged 09:52 at unapproved head 6d543ff (stale auto-merge from 00:52, P0.3).
  Post-merge review: content OK, no revert. #668 fix 0e3b40f approved + allow-protected.
- Merged since run 1: #607 (#599), #640, #645 (#612). #659 is next for OSC (version gate).
- #611 Q1: the exact clang-format fix is given (dsp.h:114 blank line + `  private:`); stop lifted.
- #627/#631 ASIO: owner claude-builder1; spec OK pending a structural fix; ci/windows added
  (no Windows run on the head); exact format fix at audio_recovery_tests.cpp:350.
- #487/#648 unassigned (builder-chat-3 retired). #629 unblocked (status/todo).

## Waiting on others
- Codex Lead: T2 of #666 (Keel's 1-line Windows docs-only CI fix,
  blocking every docs PR's windows-x64). Coda: final T2 of #659 after #645 lands.
- Compass: #663 (PLAN priority, conflicted), #665 D20, #594 v1.1 queue, Electron bump (#658 P1.6),
  Web worklet off-thread prep later phase (#658 P1.2), Random uncabled telemetry (#599).
- Quad Q1 #601/#611 → Q2 #602. ASIO #627/#631: Coda T2 + Keel spec at final head.

## Deferred / evidence boundaries
- Qt #482 R3a → #468 R3b → R5c → #481 R4 after the 3 lanes; pre-authorizations on #661 stand.
- #91 needs Keel guard/baseline authority; #560 CPU table blocked; no invented data.
- #271 hardware, #274 display, #296 3600 s soak stay distinct; accepted Pi SHA cd7999e only.
- Permanent: exact required/task/Web/Pi; no main push, force, devices/SSH or weak gates.

Signed: Keel (Claude), 2026-11-12
