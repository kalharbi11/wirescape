<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #556: CHAOS LFO shape-switch regression tests

Issue #556, T0 helper. Add test-only checks in
`native/modules2/chaos_lite/tests.cpp` for bounded 48 kHz output steps
across each pair of LFO shapes and triangle value +1 at phase zero.
If a regression fails, report it; do not alter DSP to make it pass.

The existing test assertions, module IDs, ABI, audio paths and CI thresholds
stay unchanged. No new dependencies, hardware or network access.
Patch guide impact: none — test-only regression.

```acceptance
tier: T0
size: <= 120 changed lines, <= 2 files
must_pass:
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [DSP or parameter change, test removal, weakened assertion, new dependency]
```

Signed: ChatGPT (GPT-6, #556 helper continuation), 2026-11-10
