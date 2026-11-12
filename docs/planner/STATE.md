<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12 07:45 ET, Keel (after Codex Lead's refresh).
Board #473 BODY is live; v4.1 agent/<name> ownership and Current state apply.
Only completed exact-head acceptance/checks count; queued or skipped is not proof.

## Priority and authority
- Owner order: OSC → Quad VCA/LPG → ASIO, then broad Qt retirement.
- v1.0.0 includes #597 fixes, OSC, Quad, ASIO/FlexASIO and #599.
  #593 candidate versions first; owner #581 tests its merged SHA, then tags it.
  Future app versions are owner decisions; #673 records D20 and PLAN Now.
- Seniors approve T0/T1; T2 needs the other family; Keel approves ABI/spec/CI.
- Signer identity is an auditable convention (#658 P0.2), not enforced identity.

## Immediate unblockers
- #670 merged, but its 1e-6 s soak trick still fails on hosted Windows (gpt-reviewer High).
  #678 (Keel) adds run(max_cycles=N) and tests it under a frozen clock. It blocks #673/#677
  windows-x64: needs an urgent other-session T1 review.
- #673 supersedes #665 and #663 PLAN/CLEANUP; Compass owns its integration.
  After #670, merge main normally, retain newer source/rulings, run accept664.
- #640 merged after a stale auto-merge; no revert. #636 verifier proven (CI 43215097749 green
  on 14d0db1); the schedule watch is a helper item until 2026-11-13 12:00 ET.
- Keel 07:40 ET: #676 ruled (new non-required osc_fuzz.yml, claude-builder1, after #674);
  #674 corpus → SPDX-headed libFuzzer dict; #629 moved to claude-builder1.

## OSC first
- C1 #618, Monitor #620 and row enable #645/df656cf merged; #612 closed.
- #657/#659: gpt-builder1, authored config.version absent/1 only; known enabled,
  unknown fields preserved/ignored with bounded diagnostics; spec pre-cleared.
  New head26c26c4; builder reports a repeated formatting stop. Keel supplies
  function-only clang-format correction; budget10files/320lines plus permitted
  separate formatting commit. Fresh accept657/Windows/Web and Claude T2.
- #672/gpt-builder1 follows #659: combined save/reload/presets/Clear, loopback
  row outputs/Monitor, truthful status, guide and CONTROLLERS.md, then #72.
- Keel explicitly unblocked codec fuzzing #94 before full #72 closeout;
  gpt-builder2/#674 owns it. Keel must approve workflow/Pi nightly policy.

## Quad VCA/LPG and ASIO
- #601/#611 fa7db47: gpt-builder2, CI/accept601/Static/Web now pass.
  Independent final T1 review/approval needed, then Q1 merge.
  Preserve slots0..8, four-channel/eight-voice cost, budget20files/900lines.
- #602 same owner starts only after actual Q1 merge; separate card/patch/docs T1.
- #605/#608 320f10e: scratch --profile-dir fixes Codex review Medium;
  exact CI/Static pass. Verify actual Windows isolation/accept605, then T1.
  Narrow Ninja1.13.0 Kitware suffix only; broader toolchains deferred.
- #627/#631 4e79e59: current owner gpt-builder2; Claude retry code plus GPT parts.
  CI/Static/Web pass, but latest guard FAILS; diagnose before approval.
  Preferred1..4096, quiescent prepare, one retry, no persistence; Keel spec.
  Codex reviews Claude portions; Claude reviews GPT portions; owner tests sound.

## Other continuations and permanent boundaries
- #487/#648 b1403b5: gpt-builder2; Low render_at fix pushed; exact CI/Static
  and source-matched private device-off Pi recovery evidence still govern.
- #614/#619 8f2a9d1: gpt-builder1; frozen unpatched events and exact note-offs.
- #629 after priority OSC: real WASM vs FakeEngine visual/telemetry matrix.
- #482→#468/archive→#481/#469 deferred; #661 keeps replacement/tag protections.
- #271 hardware, #274 display, #296 actual3600s, #581/#593 release are distinct.
- No new features, weakened tests, main/force push, device/SSH/LAN access.
Signed: Codex Lead, 2026-11-12
