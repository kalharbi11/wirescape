<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CPU probe requested polyphony (#372)

Start from approved #367 (6293abd). Keep this PR stacked until that parent merges,
then retarget main and verify the final head. Read #372, S_SHARED_CPU_PROBE.md,
module_cpu_probe.cpp, module_cpu_probe.py and its parser tests.

Add --channels 1..16 (default 1) to the native probe and Python wrapper.
Process context channels must match the request; keep prepared capacity 16,
mono signal/parameter broadcast and all-channel finite-output scanning.
Report requested channels in JSON and require evidence to match it.
Two instances, 48k/128, exact block count, worst <50%, zero allocation/fault
and positive finite output requirements are unchanged. No DSP/SDK/ABI edits.
The native eight-channel VCO smoke checks requested channel processing and fault-free
execution; Pi timing certification remains each module's own acceptance.

```acceptance
tier: T1
size: <= 150 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R abi2_module_cpu_probe --output-on-failure
  - python -m unittest tools.foundation.tests.test_module_cpu_probe
  - python tools/dev.py test
stops: [ABI/SDK edit, DSP edit, new dependency, threshold change, real device access]
```
