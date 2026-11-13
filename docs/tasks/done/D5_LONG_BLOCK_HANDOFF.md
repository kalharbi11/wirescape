<!-- SPDX-License-Identifier: GPL-3.0-only -->
# D5 long-block handoff deadline (#487)

Read first: #487 (signed clock-origin ruling), native/sidecar/session.cpp
Session::commit, native/control/app_engine.cpp render_live,
native/sidecar/protocol_unit_tests.cpp and docs/specs/ENGINE_PROTOCOL.md D5.

Repair the 8000 Hz / 4096-frame phase-dependent false swap refusal. The
audio interruption begins at the first callback that applies fade-out, NOT
the earlier control request. Publish a bounded render-owned callback timestamp
without realtime allocation/lock/I/O. Keep separate request/pre-mute watchdog,
existing 100 ms mute / 150 ms gap / 3-period long-block limits, candidate
fade-in deadline, stopped-stream semantics and complete rollback. Do not
change device policy, wire/ABI, tests or thresholds. No special null sink.

Verify deterministic early/late phase edges, the first-fade clock, a stalled
callback with no timestamp, successful adoption and bounded refusal; keep the
existing long-block protocol test unchanged. Real Pi device-off protocol and
recovery jobs and independent Claude-family T2 review are also required.
Protected task path is authorized explicitly in the #487 issue body.

```acceptance
tier: T2
size: <= 550 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [SDK/ABI edit, protocol wire change, tolerance or timeout relaxation, test removal, realtime allocation/lock/I/O]
```

Signed: ChatGPT (GPT-6 builder), 2026-11-11
