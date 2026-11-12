<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #460 Groove S1 release saturation and native host replacement

Read first: #454/#460 Lead rulings; GROOVE_S1_NATIVE.md;
native/document/groove_runtime_tests.cpp; native/engine2/engine2.h;
native/modules2/groove/dsp.h.

Test-only closeout stacked on #451. Retain the deliberate rejecting-sink
fixture and all prior assertions. Verify accepted note identity, port, key,
offset and absolute frame across failed OFF, retry, and following ON.
Add an actual engine graph with production ring capacities: fill critical
egress before Groove's immediate release, observe documented global choke,
then live-replan the Groove configuration while retaining a note receiver.
The old route's host choke must reach that receiver at a render boundary;
the saved-high replacement must remain stopped, with no intervening ON,
stranded note, render allocation or fault. No production source or ring,
runtime, test-count or timing threshold changes. No devices are opened.
The direct sink fixture proves pending-release retry; the engine fixture
proves the distinct host-emergency/replan recovery contract.

Patch guide impact: none — tests only; #451 carries its capability entry.
Rollback removes only the added fixture and this task, preserving #451.

```acceptance
tier: T0
size: <= 400 changed lines, <= 3 files
must_pass:
  - ctest --test-dir build/native -R 'groove|runtime_module_config|ui_catalogue_sync' --output-on-failure
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi-run: ctest --test-dir build/native -R 'groove' --output-on-failure
stops: [production source change, test removal, tolerance loosening, new dependency, size overrun]
```
Signed: Codex Lead (Codex, delegated closeout builder), 2026-11-09.
