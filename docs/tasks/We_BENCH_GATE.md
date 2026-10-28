<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-e: benchmark regression gate (#91)

**Do:** commit `benchmarks/baseline/linux-arm64-ci.json` and `benchmarks/baseline/pi5.json` from `python tools/dev.py benchmark --json` (median of 5 runs). `tools/bench_compare.py` compares a run to a baseline per graph and fails past `max_regress` (default 10% on CI VMs, 5% on the Pi). The thresholds live in `benchmarks/thresholds.json`, which `tools/ci/guard.py` protects (changing it needs `allow-protected`). CI linux-arm64 runs the compare (non-required first); the Pi nightly runs it against `pi5.json`. Updating a baseline is its own PR with the reason. Windows mono 1.05× stays tracked, not gated.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 300 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_bench_compare*.py"   # pass, regress, missing graph
  - python tools/bench_compare.py build_reports/abi2/benchmark.json benchmarks/baseline/linux-arm64-ci.json
  - python -m unittest discover -s tools/ci -p "test_*.py"   # thresholds file protected
  - ci: linux-arm64, windows-x64, guard green
stops: [threshold loosened in the same PR as a code change]
```
