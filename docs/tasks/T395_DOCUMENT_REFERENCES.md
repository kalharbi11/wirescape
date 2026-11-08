<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T395: V2 dangling document references (#395)

Independent L3 from #393. Add negative document-validation regressions for
missing cable destination modules and missing output modules; preserve the
native v2 format and all existing tests.

## Read first
- `AGENTS.md`, `docs/PLAN.md`, issue #395 and #393 handoffs.
- `native/tests/patch_document_tests.cpp`.
- `native/document/patch_document.h` and `patch_document.cpp` (read-only).
- `native/document/CMakeLists.txt` and `docs/specs/patch_v2_draft.example.json` (read-only).

## Scope
- Create an otherwise-valid two-module v2 patch with distinct source and
  destination nodes, one signal cable and a valid output.
- Assert the valid counterpart passes both `validate_patch` and `read_patch`.
- Change only the destination module id (to an absent module); assert both
  APIs reject with nonempty diagnostics.
- Independently change only the output module id; assert both APIs reject
  with nonempty diagnostics.
- No engine/format/codec/SDK, Electron, sidecar, CI, dependency or device changes.

```acceptance
tier: T0
budget: one independent builder session
size: <= 120 changed lines, <= 2 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R '^(abi2_runtime_patch_tests|abi2_staged_tests)
stops: [production change, patch format change, test removal, new dependency, device access]
```
 --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers green on the exact head
  - ci: windows-x64 Qt-enabled native test run includes document_reader_tests and patch_document_tests
stops: [production change, patch format change, test removal, new dependency, device access]
```

The optional QtCore document targets are not registered in the headless x64
acceptance runner without Qt6 >=6.5 (an observed `No tests were found`).
Do not omit their validation: the Windows CI matrix installs Qt6 and runs
`python tools/dev.py test` against these targets. The Qt-specific document
tests remain mandatory for this task on that exact head; request `ci/windows`
for the draft PR. Headless acceptance also runs available native regression
targets rather than recording a false CTest pass on an empty test selection.
