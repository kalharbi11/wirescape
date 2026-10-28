<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J7: CHAOS-lite (#74, PR #76)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "B1 / J7". One original-DSP ABI 2 module `essentials.chaos_lite` (GPL), stereo pair out, poly up to 8; comb body, coupler and drift in separate headers for M7. It will not sound identical to the legacy Surge-based CHAOS.

**Status (2026-10-28):** PR #76 green on all CI. Next: ChatGPT pre-review (T1), then the Pi number below via the Pi runner.

```acceptance
tier: T1
budget: 1 Codex session (remaining)
size: <= 1400 changed lines, <= 14 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_chaos_lite_tests" --output-on-failure
  - ci: linux-arm64, windows-x64, sanitizers, guard green on the exact head
  - chatgpt-pre-review: posted, every High fixed or answered
  - pi: two instances x 4 voices at 48000/128, worst block < 50% of the period (1333 us)
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
