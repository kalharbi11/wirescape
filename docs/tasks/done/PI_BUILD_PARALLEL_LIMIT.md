<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Honor the Pi build concurrency limit (#127)

The Pi bridge already sets CMAKE_BUILD_PARALLEL_LEVEL=2. Bare CMake
--parallel overrides that environment default. Pass the nonempty environment
value explicitly; retain native defaults when unset/empty. Do not change
network/service configuration, tests, timing thresholds or DSP behavior.

Read first: tools/dev.py command_build and reporting tests.
Regression: configured limits1/2 reach CMake, unset/empty retain defaults,
and a failed build remains a nonzero result. CMake4.2.3/Ninja reproduction
confirms bare --parallel omits -j2, while explicit --parallel2 enforces it.
This repairs load control; it does not establish why the Pi went offline.

```acceptance
tier: T0
budget: 1 Codex session
size: <= 120 changed lines, <= 3 files
must_pass:
  - python -m pip install ruff==0.14.1
  - python -m unittest discover -s tools/foundation/tests -p test_build_parallel.py
  - python -m ruff check tools/foundation/tests/test_build_parallel.py
  - ci: linux-arm64, windows-x64, guard green on the exact head
stops: [new dependency, service configuration, networking change, timing threshold change]
```
