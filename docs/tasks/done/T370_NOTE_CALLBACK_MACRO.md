<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #370 NoteToCv callback macro compatibility

Read native/voice/note_to_cv.h and note_identity_tests.cpp. Rename only callback
argument identifiers from emit to emit_event so the header compiles when Qt's
empty emit macro is visible. Preserve all callback types, signatures, note behavior,
allocation rules and IDs. Include the existing identity tests under that macro,
then undefine it before the unchanged test body. Related #361 analyzer work stays separate.

```acceptance
tier: T1
size: <= 100 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R '^abi2_(voice|note_identity)_tests$' --output-on-failure --no-tests=error
stops: [ABI/type signature change, behavioral change, analyzer suppression, threshold weakening]
```
