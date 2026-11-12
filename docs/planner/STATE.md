<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12, Codex Lead (Eastern time). Board #473 BODY is live.
Named agents retain their branches; edit Current state in place and preserve task scope.
Only completed exact-head acceptance/checks count; queued/skipped/stale is not proof.
## Priority and authority
- Owner order: OSC → Quad VCA/LPG → ASIO, then broad Qt retirement.
- v1.0.0 includes #597 fixes, OSC, Quad, ASIO and #599; #593 candidate versions,
  owner #581 tests the merged candidate SHA, then that exact SHA is tagged.
- T1 needs another session; T2 the other family. Keel retains ABI/spec/CI policy.
- Keep existing device-off/localhost and protected-path gates; no invented metrics.
## OSC closeout
- #645 row enable and #659 compatibility/version gate are MERGED.
  #659 source2fdd993 landed ase82868a with Claude review and exact-head green.
- #672 gpt-builder1 is now unblocked and STARTED branch task/672-osc-closeout.
  Combine edited/disabled/unknown/version save-reload, real loopback outputs/
  Monitor, truthful status, CONTROLLERS.md≤2KB and guide link. Then accept672,
  required checks and independent T1; close parent#72 only after actual merge.
- #94/#674 gpt-builder2: old25523fb acceptance could not invoke raw cmake/
  codec commands through the allowed runner; seed decoding and5OSC CTests pass.
  f855d15 fixes invocation; configure now proves clang++-18 absent in runner.
  Provision named compiler/runtime, then fresh60s/target proof. No workflow.
- #676 claude-builder1 follows#674: Keel's approved non-required fuzz CI/
  Pi nightly packet, with Codex T2 and Keel final CI-policy disposition.

## Quad VCA/LPG and ASIO
- Native Q1#611 merged62fa9ec; setup#608 merged3a3bd34.
- Q2#675 source57b48dc MERGED0cc86f76 after independent Codex T1 and all
  required/applicable gates plus actual selectedaccept602 completed. Parent
  #490 and child#602 are closed. Both production library renders pass.
- #627/#631 owner gpt-builder2: Keel authorized600lines/10files and unchanged
  spec. Both original Codex Highs source-fixed: producer join/rebind before
  graph replacement; valid UTF-8-safe bounded diagnostics/status/refusals.
- Codex narrow9576c82 fixed strict task-size syntax and enabled the existing
  paced fake callback for live OSC loadGraph; local task parser passes.
  New b71a16e fixes codec fixture capacity256→1024; selectedaccept627 native
  build/test and Linux/TSan pass. Windows/ASan/Static/Web still running.
  Codex Claude-retry delta posted; Claude must review GPT fixes/test repair.
  Only owner checks physical ASIO sound; close#610 after#631 actually merges.

## Supporting work and decisions
- #619/#614 source014b14e: final T1 source pass found no new High/Medium;
  frozen byte/event trace, exact note-offs at8/12samples and input precedence.
  Complete current acceptance/required/Static/Web; OSC closeout stays first.
- #648/#487 now97f9e63: format-only changes and normal main integration;
  source-matched private device-off Pi run still required, then Claude T2.
- #6791974969: green CI but reviewer High remains: Guard completion can miss
  a later auto-merge when cron is dead. Keel owns durable wakeup/race proof.
- #681bbca518 Keel board fast path: senior Medium on raw closing-directive
  regex consuming Markdown examples; Keel fixes/disposes. No policy bypass.
- #636 helper watch: manual main_verify43215072597→CI43215097749 green on
 14d0db1; by2026-11-13 noonET link schedule or report it never fired.
- #629 claude-builder1 audits real WASM/FakeEngine parity after urgent reviews.
## Permanent boundaries
- #482→#468/archive→#481/#469 remains deferred; #661 preserves replacement gates.
- #271 hardware, #274 display, #296 actual3600s, #581/#593 release are distinct.
- No weakened tests, main/force push, devices, LAN or SSH.
Signed: Codex Lead, 2026-11-12
