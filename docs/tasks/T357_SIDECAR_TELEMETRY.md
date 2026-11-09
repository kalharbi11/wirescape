<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #357 A2: sidecar engine2 telemetry taps

**Contract:** `docs/design/ENGINE_TELEMETRY.md`, all rulings; shared
brief `docs/tasks/T350_ENGINE_TELEMETRY.md`, issue #357.

Install taps on the quiescent candidate before prepare. Map exactly the
modulation-cabled parameters, ADSR/Function Envelope `envelope` output
and event output ports. The control owner publishes complete snapshots,
empty on a non-advancing frame. Reset last-seen event-count cursors and
pulse/decay state on each successful replan and D5 commit, including
`loadGraph`; verify the first burst after both edits.

Retain protocol v0, document optional `outputs`, and keep `phase` empty
until B-wire #359. Do not read module-internal DSP state.

**Read first:** `native/sidecar/session.cpp`,
`native/document/runtime_patch.cpp`, `native/sidecar/protocol_tests.py`.

**Further gates:** P1H conformance golden, Pi 64-tap/no-tap render
comparison, and independent Keel or Compass cross-family T2 review.

```acceptance
tier: T2
size: <= 450 changed lines, <= 9 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
stops: [lock/allocation/I/O/JSON in render, telemetry above 30 Hz, reporting unrendered values, Pi regression beyond tolerance]
```

Signed: ChatGPT (GPT-6, sidecar telemetry builder), 2026-11-07 (America/New_York).
