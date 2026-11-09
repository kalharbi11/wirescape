<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Macro Oscillator in the app catalogue (#162)

Register the existing `ports.macro_oscillator` through the single builtin list and
propagate its existing `macro_upstream` include/link context through the catalogue
interface target. Preserve the 18 existing entries and every original DSP/oracle
test, ID, parameter and default. No new source, dependency or upstream patch.

Read issue #162, `native/control/module_catalog.{h,inc}`,
`native/engine2/CMakeLists.txt`, `native/control/app_engine_tests.cpp`,
`native/document/app_patch_tests.cpp`, and the Macro README/DSP/CMake registration.
Add an inline real-loader keyboard pitch/gate/velocity-to-Macro fixture and output
connection; assert finite nonzero rendering, zero callback allocation/faults, and
clear rejection at 44.1/96 kHz. Macro remains 48 kHz only, eight active voices;
see `native/modules2/macro_oscillator/README.md`. Tests use synthetic input only.

```acceptance
tier: T1
budget: 1 bounded integration pass
size: <= 260 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "^(app_engine_tests|abi2_app_patch_tests|abi2_macro_oscillator_tests)$" --output-on-failure --no-tests=error
  - ci: windows-x64 (including Qt offscreen tests), linux-arm64, sanitizers, guard
  - pi-run: ctest --test-dir build/native -R "^(app_engine_tests|abi2_app_patch_tests)$" --output-on-failure --no-tests=error
stops: [rate policy change, dependency or ABI change, ID or state change, test removal, threshold change, budget exceeded, physical device access]
```
