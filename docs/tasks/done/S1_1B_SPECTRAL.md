<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S1-1b (#305): sound.spectral, PR B (Interleave and Vocoder)

Follows #302 / PR #304. Adds mode 6 Interleave and mode 7 Vocoder to `sound.spectral`. Parameters are appended at slots 9–15 as confirmed in #302, and state goes to version 2 (version 1 still loads). Plan and behaviour tests: issue #305.

**CPU stress scheduling:** At 48 kHz / 128 frames / size=3, the FFT hop is 1024 samples (8 probe blocks). The `bands=6:64:8` test changes once per hop, and `mode=0:7:32` holds each mode for four hops, enough to exercise the transition fade. Faster 1- or 4-block alternation would alias to a single value at hop boundaries and falsely certify steady-state performance.

```acceptance
tier: T1
# Budget: <= 900 changed lines / <= 12 files (issue #305); generated files excluded.
size: <= 900 changed lines, <= 12 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "^abi2_spectral_tests$" --output-on-failure --no-tests=error
  - python tools/gen_ui_catalogue.py --check
  - python tools/dev.py test
  - pi-run: python tools/module_cpu_probe.py sound.spectral
  - pi-run: python tools/module_cpu_probe.py sound.spectral --param mode=7 --param size=3 --alternate bands=6:64:8
  - pi-run: python tools/module_cpu_probe.py sound.spectral --param size=3 --param pitch=1 --alternate mode=0:7:32
stops: [ABI header edit, new dependency, file I/O in a module, test loosening]
```

## Current-main synchronization and evidence gate (2026-11-08)

PR #310 now targets `main`. Its integration merge `939bb80` preserved the
current 27-module catalogue and the accepted cppcheck/allocator test fixes;
the intended Interleave/Vocoder diff remains nine task-scoped files.
The retarget happened after that branch push, and repository CI triggers on
`pull_request.synchronize`, not on PR base-edit events. This documentation
synchronization requests a fresh exact-head run without altering DSP.
Earlier-head test results are not new-head acceptance. Keep the draft open
until the tests above, Windows/Linux/static, task-specific Pi probes and an
independent T1 review all cover the eventual head.
