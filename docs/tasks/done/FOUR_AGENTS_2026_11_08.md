<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Four existing chats: updated continuation, 2026-11-09

Owner explicitly requested the same four agents/chats. These are paste-ready continuation instructions, not claims that chats have received messages automatically. Canonical copies were posted on #383; latest issue handoffs take precedence. No new chats or schedules.

## Agent 1: Sequencer and starter integration

Agent 1 — repo kalharbi11/wirescape; continue existing branches and latest issue claims. Read AGENTS/task acceptance and docs/sequencer/README.md. #420 SDK64 is MERGED84f926b, do not redo. Native #401/PR405 task/401-step-sequencer-16 current960ec61 -> #408/PR410 task/408-seq16-ui-v2 currente151680 AFTER native merges. Preserve default8/statev1, IDs, noteoff retry and curated eight STARTERS; regenerate ui_starters.json from actual exports and native405cppcheck b.emit parse atdsp.h:90 needs narrow macro restoration; format tests.cpp:376 and rerun. Fix actual conformance/layout failures, no assertion removal. Run python tools/dev.py accept 401 / 408 --file <actual-task> and native/Studio/Pi must_pass; independent T1 on final head. Groove has its own builder: coordinate regressions, do not take over #451. Max3active, oneissue/branch/PR, exact-head signed handoff.

## Agent 2: Finish the module lane

Agent 2 — repo kalharbi11/wirescape; Reverb348 and Delay345 are MERGED, no repeat integration. Quantizer #315/PR323 task/315-quantizer current385ad20: prior selected/required/Studio/WASM PASS, no duplicate DSP; diagnose current main conflict, normal merge preserving all descriptors, narrow independent review and new-head gates. Old static had only unchanged host_keyboard parser diagnostic; lead classifies, never disable checker. Then #305/PR310 task/305-spectral-b current83a4b05 already targets main: Interleave first; current T1 no High/Medium, hosted native/Web PASS. Verify actual3exact-head Pi probes, inherited-only static ruling and any new integration. Preserve ASan partial-block/hop-boundary tests, modes/slots/statev1/v2, run accept315/305 and actual native/Studio/WASM commands. Patch guide reflects only merged capability. Max3active; exact-head signed handoff.

## Agent 3: MIDI completion

Agent 3 — repo kalharbi11/wirescape; continue #381/PR390 task/381-midi-learn current6a8be72 and #290/#163/PR210 task/163-midi-patchable currentdf81486. #390 hosted/static/Web/WASM green; Keel final T2 helper-refactor review still required, no duplicate seam. #210 actual required/Pi pass and independent T1 no correctness finding; fix authored formatting and classify803cppcheck diagnostics against baseline, then new-head evidence/review. Preserve Groove config, last_note slot8, CC ranges/inversion/smoothing/configv1/v2, original synthetic soak. Run actual discovered task (accept163 --file docs/tasks/J_MIDI_PATCHABLE.md for210), not guessed issue-title selection. #287 starts only after actual381merge: persisted cc_to_cv rows, opaque data, cancel/expiry/disconnect/row-exhaustion/reload. Chord406/Notes423 and audio399 remain Keel contract-gated, no speculative implementation. Max3active; exact-head signed handoff.

## Agent 4: Audio evidence and Pi soak

Agent 4 — repo kalharbi11/wirescape; #385/PR398 render evidence is MERGED, don't redo cap repair. Continue #386/PR442 task/386-electron-sidecar-e2e current1716e2a in existing harness: hosted3cases green; preserve save/reopen unknown data, refusal rollback/generation, numeric native telemetry; fourth device-recovery case waits Keel safe injection ruling, no invented wire/test-only protocol. Linux/Xvfb functional evidence does not certify Windows. In parallel #296 private fixed3900s edit-soak lane: inspect latest private claim/policy, independent Claude T2 and actual3600s fake-device source-head evidence, zero crashes/overruns/faults; queued runs not passes. #387 reduction waits385+386+296. No Pi installation/setup or439work. #427 is owner-auto-merge-enabled but OPEN at464c042; existing resolver owns it, verify new-head gates then actual main/live8patch deployment. Max3active, exact-head signed handoff.

Other existing owners: Groove454/45131e0aa8 fixes102cycle undrained-egress fixture and separate saturated NOTE_OFF/reload-pending proofs, then format/accept454/Pi/T1; UI432 waits designer + Keel STOP/playhead ruling. OSC440/441 inventory is implemented/hosted green; parent72runtime waits Keel ownership/consent/Monitor contract, not staleQt82merge. Latency307/400 formatting then T2; Macro435/438 final lifetime/baseline ruling. Details: CLAIMABLE_2026_11_09.md and latest393handoff.

Signed: Codex Lead (Codex), 2026-11-09

```acceptance
tier: T0
size: <= 270 changed lines, <= 2 files
must_pass:
  - python -c "from pathlib import Path; assert len(Path('docs/planner/STATE.md').read_text().splitlines()) < 60; assert sum(l.startswith('## Agent ') for l in Path('docs/tasks/FOUR_AGENTS_2026_11_08.md').read_text().splitlines()) == 4"
stops: [phase-goal change, protocol change, source change]
```
