<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio Clock Start/Stop (#151)

Owner-authorized demo task. Show an accessible Start Clock / Stop Clock toggle for **only** the existing `essentials.clock` parameter `run` (stable ID 3, values 0/1) on the module card. Keep the generic parameter slider for every other module and parameter. Use `studio.setParameter` and its accepted `parameterChanged` event as source of truth; rejection must not visually change the state.

Read first: `native/modules2/clock/module.json`, `native/modules2/clock/dsp.h`, `native/ui/qml/ModuleCard.qml`, `native/ui/qml/ParameterControl.qml`, and `native/ui/tests/ui_tests.cpp`. Never change the clock DSP, phase, reset, gate semantics, defaults/ranges, IDs, patch schema, dependencies, or global audio transport. Clock run=0 gates its output low; run=1 resumes its existing phase. This does not implement external MIDI clock.

Offscreen Qt tests should click and keyboard-toggle twice, verify actual 0/1 values, distinguish Clock run ID 3 from other module IDs, and confirm accepted external updates and rejected-write rollback. Devices remain off. Do not merge/overlap #147 control polish; limit changes to the four listed task/UI/test files.

```acceptance
tier: T1
budget: bounded ChatGPT task
size: <= 200 changed lines, <= 4 files
must_pass:
  - ci: windows-x64 green on exact head, with studio_ui_tests and abi2_clock_tests passing
stops: [ABI header edit, new dependency, format or ID change, DSP change, tests removed, unapproved threshold change, >4 files]
```
