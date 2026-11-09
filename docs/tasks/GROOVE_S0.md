<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #430 Groove S0: stopped structured-config proof

Authorized by #430, child of #429. Owner revised the build order on
2026-11-08. Read AGENTS.md, latest #430 handoff, docs/PLAN.md Now/Decisions,
docs/planner/STATE.md, and docs/sequencer/{README,GROOVE}.md on
task/429-sequencer-roadmap until #431 merges.

## Read first and bounded scope
- docs/SDK_AUTHOR_GUIDE.md §§7–12, native/sdk/examples/step_sequencer/
- native/document/{runtime_patch.cpp,runtime_module_config.h}
- native/control/{module_catalog.h,module_catalog.inc}
- native/modules2/midi_cc_to_cv/config.h, native/document/builtin_source_tests.cpp
- apps/studio/src/patch/{PatchDoc,History}.ts, apps/studio/src/engine/RealEngine.ts
- tools/gen_ui_catalogue.py, docs/PI_RUNNER.md

Create `sequencing.groove` with a small versioned module.config (1 track,
at most eight steps) and off-render validation/adapter. Prove native
load, audio-frame transitions, NOTE_ON/OFF and pitch/gate, refuse
bad candidates and round-trip authored JSON through save/reload.
Load stopped and suppress accidental note-ons; source is a fixed-size
module, no scalar cap increase and no callback allocations/JSON/locks/I/O.
Preserve existing example.step_sequencer and #401/#405/#408/#410,
#403/#406/#421 ownership. No live-edit command, patch-edit UI,
new dependency or new ABI. S1 full instrument belongs to a separate issue.

`sequencing.groove` format/IDs must receive cross-family Keel T2 review.
The module DSP itself is T1 only after a reviewed contract.
S0 does not claim S1 transport, UI loaded/refused state, mods or
physical sound. Author explicitly notes those in the handoff.

## Evidence and rollback
Tests exercise 44.1/48/96 kHz, blocks 1/17/64/128/256, native
configured render, strict invalid config refusal, note release and
repeatable loop, allocation watcher and reload equality. Capture
actual required Linux/Windows/sanitizer and private Pi evidence.
No queued Pi run and no FakeEngine result counts as native evidence.
Only owner tests physical Pi audio.

Rollback removes module registration/adapter/catalogue/test as one unit,
**not** the user's authored patch JSON. Unknown modules remain preserved
and unavailable; never silently discard data.

```acceptance
tier: T2
size: <= 850 changed lines, <= 15 files
must_pass:
  - ctest --test-dir build/native -R 'groove|runtime_module_config|ui_catalogue_sync' --output-on-failure
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi-run: ctest --test-dir build/native -R 'groove' --output-on-failure
stops: [ABI edit, unreviewed T2 schema, new dependency, threshold relaxation, size overrun]
```
