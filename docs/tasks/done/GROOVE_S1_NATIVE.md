<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #454 Groove S1(a): independent native two-track instrument

Keel preauthorized `docs/tasks/GROOVE_S1*.md` in #432 comment 6731562789.
S0 #430 is merged; parent #432 retains the separate S1(b) Studio editor.
## Read first
AGENTS.md; #432 ruling/handoff; docs/PLAN.md Now/Decisions; docs/sequencer/{README,GROOVE}.md;
docs/SDK_AUTHOR_GUIDE.md §§7–12; native/modules2/groove/{config.h,dsp.h,module.json};
native/document/{runtime_module_config.h,groove_runtime_tests.cpp}; native/sdk/examples/step_sequencer/dsp.h.
## Scope and rollback
Preserve JSON `version:1`, S0 `run` and `t1_*` IDs, native 2×32
(shared audio-frame clock) with rest/note/tie, velocity, pitch/gate/
matched notes, two sample-held mods per track, queued global/track
start/stop, immediate mute and reset precedence. Validate quiescently,
reserve 4×64 POD, no live-edit, ABI, protocol, new dependency or UI changes.
Tests must cover all matrix rates/blocks, 100+ loops, refusal/reload,
transport, release overflow and no callback allocation.
Rollback removes new registration/adapter but preserves authored JSON,
marking unavailable; never rewrite saved data. If authoritative native
STOP/playhead is unavailable, request Keel T2 ruling before editor work.

Owner-authorized Lead closeout accounting (#454, 2026-11-09): the semantic
implementation remains capped at900 lines. The975-line total allows only
pinned LLVM18 whitespace formatting, the mandatory central patch guide entry
and this ruling; all assertions and runtime thresholds remain unchanged.

```acceptance
tier: T1
size: <= 975 changed lines, <= 20 files
must_pass:
  - ctest --test-dir build/native -R 'groove|runtime_module_config|ui_catalogue_sync' --output-on-failure
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi-run: ctest --test-dir build/native -R 'groove' --output-on-failure
stops: [ABI/protocol edit, unreviewed format/transport, new dependency, test removal, tolerance loosening, size overrun]
```
Signed: ChatGPT (GPT-6, Groove S1 native builder), 2026-11-08.
