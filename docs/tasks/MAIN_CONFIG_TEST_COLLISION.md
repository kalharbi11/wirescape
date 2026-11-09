<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #485 Repair the combined MIDI/Groove runtime-config test collision

Read first: issue #485; PR #486; native/document/runtime_module_config_tests.cpp.

On combined main, both the MIDI legacy fixture and the Groove legacy fixture
declare old in the same function. Rename only the Groove fixture and its decode
reference. Preserve both legacy configurations and every assertion.

Task is T0: test-only identifier repair. No production code, test removal,
CI change, threshold change or device access. Hosted build/CTest is the compile
evidence; no Pi hardware result is implied. Combined Pi setup remains #483.

Signed: Codex Lead (Codex), 2026-11-09.

```acceptance
tier: T0
size: <= 80 changed lines, <= 2 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R '^runtime_module_config_tests$' --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on the exact head
stops: [production change, test removal, threshold change, device access]
```
