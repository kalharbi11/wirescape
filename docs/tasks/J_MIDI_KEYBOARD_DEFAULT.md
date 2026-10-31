<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Interactive MIDI keyboard and input diagnostics (#184)

Implement issue #184 from owner field report #182: distinguish endpoint identities,
show per-port live receive counts and system names, route enabled inputs to the
patch keyboard by default, and persist an explicit interactive Enable MIDI choice.
Preserve existing midi.in/converter routing and stable IDs. Tests, smoke/startup
checks and probes remain device-disabled even if interactive preferences exist.
Panic, disconnect and reconnect must release held notes. No new backend or ABI.
Read midi_host, io/midi service/backend, host_keyboard and Studio controller/main.
File-cap self-decision: WORKFLOW §3c permits +50%; 10 → 15 includes the fake
service/Host/UI tests and the interactive/offscreen consent boundary. Line allowance:600 →900 under WORKFLOW3c to cover independent-review
probe-safety and threaded disconnect/reconnect regressions; file cap remains15.

```acceptance
tier: T2
budget: 2 bounded implementation passes
size: <= 900 changed lines, <= 15 files
must_pass:
  - ctest --test-dir build/native -R "^(abi2_midi_service_tests|midi_host_tests)$" --output-on-failure --no-tests=error
  - ci: windows-x64 fake-backend/controller tests: distinct endpoints, independent receive counts, global keyboard note, persisted preference and panic
  - ci: linux-arm64, sanitizers, guard
  - user: Launchkey notes sound on Windows and Pi
stops: [ABI header edit, device access in tests, ID change, new dependency]
```
