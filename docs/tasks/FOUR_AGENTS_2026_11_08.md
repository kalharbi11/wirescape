<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Four existing chats: direct continuation, 2026-11-08

Owner explicitly requested the same four agents/chats. These are paste-ready continuation instructions, not claims that chats have received messages automatically. Canonical copies were posted on #383; latest issue handoffs take precedence. No new chats or schedules.

## Agent 1: Sequencer and starter integration

Agent 1 — continue your existing chat. Repo kalharbi11/wirescape. Read current AGENTS.md, latest handoffs and task acceptance. #412 is now MERGED3af48ae; no more patch-design work. Queue: (1) #401/PR405 existing task/401-step-sequencer-16: integrate actual main/cap64/curated starters; format ONLY authored step_sequencer/dsp.h and tests.cpp with clang-format18 (old diagnostics27+23), inspect cppcheck separately, fix actual Studio acceptance failure; keep8-step default/statev1 and noteoff retry tests. (2) After native merge, #408/PR410 existing branch: integrate parent and regenerate native/document/ui_starters.json from ACTUAL STARTERS; Web's codec-roundtrip failure is the ADSR y1000 vs660 mismatch, not a timeout. Preserve new eight curated starters, adapt the old sequence-specific layout edit since that generic starter is gone. Diagnose native builtin_source/ui_patch_conformance failures; no deleted assertions. (3) #418/PR420 only when a slot is free: main conflict reconciliation in tests, narrow review then green gate; prior exact-head checks allgreen. Run python tools/dev.py accept401/408/418 respectively plus mandated Studio/native/Pi acceptance; one issue/branch/PR each, max3active. Done: current-head native+UI green, independent T1, correct migration/fixtures; hand off exact SHA. Sign your model/temporary role/date.

## Agent 2: Finish the module lane

Agent 2 — continue existing chat. Repo kalharbi11/wirescape. #345 Delay and #379 LFO phase are now MERGED; do not redo them. Finish sequentially: #316/PR348 on task/316-reverb, then #315/PR323 existing branch, then #305/PR310 Spectral Interleave/Vocoder. Resolve main conflicts via normal merges, retain Delay/Spectral/Resonator/Blend and every other descriptor, use Python generator for catalogue bytes and update actual-count assertions. Reverb has prior exact-head Pi evidence; new compiled conflict resolutions need applicable fresh proof. Quantizer currently has no task pi-run requirement; follow actual acceptance. #310's base304 is already merged: retarget main and integrate normally, no history rewrite. Run python tools/dev.py accept316,315,305 and each task's native/Studio/Web-demo/CPU commands. Get independent current-head T1 review. No DSP redesign, suppressed warnings or threshold changes. Final result must appear in shared native/Studio/WASM catalogue and survive patch save/reload. Max3active, oneissue/branch/PR. Report exact head, actual checks, blockers, and sign model/temporary role/date.

## Agent 3: MIDI completion

Agent 3 — continue existing chat. Repo kalharbi11/wirescape. Queue: #381/PR390 on task/381-midi-learn, #290/#163/PR210 on task/163-midi-patchable, then #287 Learn UI after381actually merges. Both existing PRs conflict; normal main merges, preserve protocol/fixture additions and stable IDs; no second Learn seam. PR390 task13files/400lines allowance already synchronized; do not repeat budget work. Obtain Keel's new-head cross-family review for callback helper refactor; required Windows/native/mock-bridge/acceptance must be exact-head. PR210 retain last_note/raw pitch and CC range/configv1→v2 tests and task-specific Pi evidence. UI287 writes cc_to_cv rows per D8 and persists/reloads, with cancel/expiry/disconnect safety. Read each task then run python tools/dev.py accept381/290/287 and its actual must_pass. Chord403/421 is a separate Keel-gated route, not raw-MIDI Learn work; don't implement speculative fields. After one slot opens take owned route293/399 if Keel rules, preserving no implicit audio fallback. Max3active; oneissue/branch/PR; exact-head signed handoffs, no devices/SSH/threshold waiver.

## Agent 4: Audio evidence and Pi soak

Agent 4 — continue existing chat. Repos kalharbi11/wirescape and private wirescape-pi. First #385/PR398 existing task/385-offline-render-evidence: Codex Lead reviewed renderer and found all5 native CTests plus780e280 exact-head Pi acceptance PASS; only task size failed553>500. I pushed DOCS-ONLY3f12baa cap560 within already documented+50% workflow scope allowance, keeping10files/all thresholds. Do not redo that repair. Verify fresh selected acceptance, required/static checks and artifact provenance; reuse docs-only compiled/Pi evidence only where rules permit. Once merged, #386 real-sidecar Electron E2E (save/reopen/rollback/reconnect/real telemetry), with exact seam/platform ruling from Keel. In parallel continue #296 private fixed ci/pi-edit-soak lane: public389 prerequisite already merged, fixed3900s timeout only, no generic timeout change, independent Claude T2 review; then actual3600s fake-device evidence on source head, zero crashes/overruns/faults. No private PR exists yet at this read; create one only after inspecting current private policy/owner handoff. #387 owner-checklist reduction waits385+386+296 evidence. Run each task's must_pass/pi-run, not queued runs. Max3active, oneissue/branch/PR, no devices/SSH, exact evidence links and signed model/role/date handoffs.

Signed: Codex Lead (Codex), 2026-11-08

```acceptance
tier: T0
size: <= 270 changed lines, <= 2 files
must_pass:
  - python -c "from pathlib import Path; assert len(Path('docs/planner/STATE.md').read_text().splitlines()) < 60; assert sum(l.startswith('## Agent ') for l in Path('docs/tasks/FOUR_AGENTS_2026_11_08.md').read_text().splitlines()) == 4"
stops: [phase-goal change, protocol change, source change]
```
