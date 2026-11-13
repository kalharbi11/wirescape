<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS native ↔ Studio descriptor/panel regression (#575)

T0 test-only helper. Read #575 and the merged #533 LFO-shape compatibility
requirements. Compare `native/modules2/chaos_lite/module.json`, generated
`apps/studio/public/wirescape_modules.json`, and
`apps/studio/public/panels/essentials.chaos_lite.json`.

Check internal ID `essentials.chaos_lite`, the generated display name
matching the current native descriptor **without hard-coding a name**, panel
code **CHAOS**, stable prior 26 parameter slots, appended `lfo_shape` slot 26 with sine
default zero and four advertised choices, and panel IDs present in catalogue.
Prove a mutated fixture fails. Do not edit native DSP, Studio app, generated
files or presets, and do not resurrect the dropped #565 PERFORM UI.
Patch guide impact: none — test-only existing product contract.

```acceptance
tier: T0
size: <= 130 changed lines, <= 2 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_ui_catalogue.py
  - python tools/gen_ui_catalogue.py --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [production changes, generated catalogue edit, module/parameter IDs, new threshold or dependency]
```
