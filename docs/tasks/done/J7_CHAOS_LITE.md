<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J7: CHAOS-lite (#74, PR #76)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "B1 / J7". One original-DSP ABI 2 module `essentials.chaos_lite` (GPL), stereo pair out, poly up to 8; comb body, coupler and drift in separate headers for M7. It will not sound identical to the legacy Surge-based CHAOS.

The executable fixture is `native/examples/chaos_two.json`. J2's keyboard
source supplies four pitch channels; unconnected CHAOS gate/velocity inputs use
their documented defaults. The app-loader test asserts both instance channel
counts, finite non-silent output and no callback allocations. The runner probe
uses 120 seconds and enforces the existing worst-block bound, not just p99.

**Status:** The module shipped in merged PR #76. The executable fixture
follow-up is stacked on J2 for its four-channel keyboard source; its current-head
T1 pre-review and Pi acceptance are required before merge.

```acceptance
tier: T1
budget: 1 Codex session (remaining)
size: <= 1400 changed lines, <= 14 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_chaos_lite_tests|app_patch_tests|app_engine_tests" --output-on-failure
  - python -m unittest discover -s tools/foundation/tests -p "test_chaos_two_probe.py"
  - ci: linux-arm64, windows-x64, sanitizers, guard green on the exact head
  - chatgpt-pre-review: posted, every High fixed or answered
  - pi-run: python tools/chaos_two_probe.py   # two instances x 4 voices at 48000/128; worst block < 50% of period (1333 us)
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
