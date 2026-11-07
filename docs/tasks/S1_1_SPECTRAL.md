<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S1-1 (#302): sound.spectral, PR A (STFT engine; Morph, Cross, Max, Mean, Min, A − B)

Brief: `docs/tasks/S1_CONJUNCTIONS_MODULES.md` (module 1). The plan, IDs and Keel/Compass rulings are in issue #302. PR A ships the engine, modes 0–5, the catalogue wiring and the generic CPU probe (`tools/module_cpu_probe.py`). Interleave and Vocoder (modes 6–7, parameter slots 9–15) follow in their own issue and PR.

```acceptance
tier: T1
# Budget (Compass, #302): module folder <= 1400 lines / <= 10 files; wiring, generated files and the probe are excluded.
# The checker counts the whole diff, so this line holds the whole-diff totals.
size: <= 1800 changed lines, <= 24 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "^(abi2_spectral_tests|abi2_module_cpu_probe_smoke|app_engine_tests)$" --output-on-failure --no-tests=error
  - python tools/gen_ui_catalogue.py --check
  - python tools/dev.py test
  - pi-run: python tools/module_cpu_probe.py sound.spectral
stops: [ABI header edit, new dependency, file I/O in a module, test loosening]
```
