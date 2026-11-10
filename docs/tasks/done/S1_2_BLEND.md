<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S1-2 (#328): sound.blend (equal-power crossfade and splice)

Brief: `docs/tasks/S1_CONJUNCTIONS_MODULES.md` (module 2). Design, IDs and tests: issue #328.

```acceptance
tier: T1
# Budget: module folder <= 900 lines / <= 10 files; wiring and generated files excluded (Compass, #302).
size: <= 1200 changed lines, <= 18 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "^(abi2_blend_tests|app_engine_tests)$" --output-on-failure --no-tests=error
  - python tools/gen_ui_catalogue.py --check
  - python tools/dev.py test
stops: [ABI header edit, new dependency, file I/O in a module, test loosening]
```
