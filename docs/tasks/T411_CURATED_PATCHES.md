<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T411: Curated cinematic starter library (#411)

Owner request: keep the six historical Qt session bundles, the three Pi/M8
class-demo JSON files and the default keyboard-voice smoke-test patch intact.
Replace only five generic interactive starters with eight self-contained v2
presets. Do not delete user-saved patches or modify DSP/ABI/module IDs.

The authored library includes three CHAOS studies, three distinct Macro
Oscillator model uses, a stock-module drone, and two independent self-running
eight-step progressions. Each entry must have legal module/port/parameter
references, stereo outputs and stable round-trip data. The desktop Studio,
browser demo and real native engine must load/render the graph. The audio
worklet requires an explicit user gesture; no browser autoplay bypass.

```acceptance
tier: T1
budget: one curation and verification session
size: <= 900 changed lines, <= 12 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R '^(ui_patch_conformance|abi2_app_patch_tests|abi2_example_step_sequencer_tests|abi2_example_random_source_tests)$' --output-on-failure --no-tests=error
  - python tools/gen_ui_catalogue.py --check
  - ci: guard, linux-arm64, windows-x64, sanitizers, static, Web and Web demo green on exact head
  - chatgpt-pre-review: independent T1 review of native and WASM patch behavior before merge
  - user: listen to all eight patches and approve CHAOS movement, levels and macro timbres
stops: [destructive user-data removal, unverified module IDs, DSP or ABI edits, incorrect autoplay claims, missing independent review]
```
