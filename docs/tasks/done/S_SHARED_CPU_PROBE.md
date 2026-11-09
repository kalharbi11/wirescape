<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Shared module CPU probe extraction (#366)

## Goal
Move the reusable offline CPU probe out of Spectral PR #304 so Reverb (#316), Delay (#314),
and Resonator can collect independent Raspberry Pi timing evidence without depending on
the Spectral engine. This task changes tooling only; it does not certify any module's Pi timing.

## Read first
- `native/engine2/CMakeLists.txt` and `native/control/module_catalog.h` on main
- `native/engine2/tests/module_cpu_probe.cpp` and `tools/module_cpu_probe.py` in #304
- `tools/foundation/tests/test_module_cpu_probe.py` in #304

## Scope and contract
- Add a Qt-free, device-free C++ probe for **any built-in module**. Two independently
  prepared instances receive a reproducible 110 Hz saw into signal input 1, seeded
  noise into input 2, and silence on remaining inputs. Inputs are prepared outside
  the timed callback; allocations/faults are observed inside it.
- Emit exactly one `module_cpu_probe ` JSON evidence line with module ID, instances,
  rate, frames, duration, block count, max/p99 render time, expected period, callback
  allocation/fault counts, and finite output peak. Retain `--param` and
  `--alternate` to exercise expensive static modes and repeated parameter changes.
- `python tools/module_cpu_probe.py <module-id>` enforces 2 instances, 48 kHz /
  128 frames, worst block strictly below half the 2.667 ms period, **zero**
  audio-thread allocations/faults, and finite non-silent output. Default 60 s.
  The pass rule is unchanged from #304.
- CTest smoke uses `sound.blend`, already present on `main`; a stress smoke
  flips `mode` and overrides `balance`. Add parser/threshold regression coverage.
- Do not import any Spectral DSP, alter ABI/module IDs or audio behavior, open
  physical devices or add a dependency. Keep the `sound.spectral` acceptance
  cases on #304 after this tooling PR merges.

The all-channel finite-output regression adds two small test/helper files (5→7),
within Workflow v3's +50% file allowance. Numerical thresholds are unchanged.

## Usage
`python tools/module_cpu_probe.py sound.blend --seconds 10 --param balance=0.25 --alternate mode=0:1:4`

Other modules use their catalogue IDs and parameter IDs. Run on the Pi runner to
record hardware-specific evidence; local desktop timings are not Pi certification.

```acceptance
tier: T1
size: <= 550 changed lines, <= 7 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R abi2_module_cpu_probe --output-on-failure
  - python -m unittest tools.foundation.tests.test_module_cpu_probe
  - python tools/dev.py test
stops: [ABI edit, new dependency, threshold change, real device access]
```
