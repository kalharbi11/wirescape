<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #513 Windows null-sink scheduling and overrun diagnostics

Read first: issue #513 diagnosis and Keel ruling; native/sidecar/session.{h,cpp};
native/sidecar/protocol_unit_tests.cpp; native/sidecar/protocol_tests.py;
tools/sidecar_edit_soak.py; tools/foundation/tests/test_sidecar_edit_soak.py.

Implement Keel's approved correction: Windows null-sink MMCSS "Pro Audio" with
TIME_CRITICAL fallback and sink-lifetime timeBeginPeriod(1), paired cleanup.
No devices, LAN, dependencies or CI changes. Keep absolute deadlines, catch-up
blocks, zero-overrun assertion and all existing thresholds. Fixed-size counters
and maxima distinguish overrun render duration from positive scheduled-start
lateness; report after join, preserve diagnostics in soak failures. Counts may
overlap. Regression covers equality at deadline, delayed wake with short render,
long render, catch-up, no render allocation, malformed diagnostics and strict
failure. Windows execution and cross-family Coda review remain required.

Patch guide impact: none — no patch-facing capability changes.
Rollback reverts scheduling and reporting together; no data migration.

```acceptance
tier: T2
size: <= 650 changed lines, <= 10 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_sidecar_edit_soak.py
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [test weakening, ABI header edit, new dependency, device access, LAN access, CI change, size overrun]
```
