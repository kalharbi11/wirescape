<!-- SPDX-License-Identifier: GPL-3.0-only -->
# 2B-c (#288): MIDI hot-plug

Read issue #288, native/sidecar/DESIGN.md, ENGINE_PROTOCOL device/status
semantics, PLAN D7 and the existing MIDI host/service/backend tests.
Detect removal and return on the existing non-realtime worker. Release only
the removed source's held notes, restore subscriptions on return, push device
lists and preserve remembered endpoint enable choices. Fake devices only.
Coordinate session.cpp integration with audio-loss #291. No ABI, format,
protocol command, dependency or realtime allocation/lock/I/O changes.

```acceptance
tier: T2
size: <= 500 changed lines, <= 8 files
must_pass:
  - ctest --test-dir build/native -R "midi|sidecar_" --output-on-failure --no-tests=error
  - pi: same fake-device MIDI/sidecar regressions on pi5, exact PR head
stops: [ABI header edit, ID/format change, new dependency, real devices in tests, threshold loosening]
```
