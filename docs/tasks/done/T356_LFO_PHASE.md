<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #356 B-mod: add the LFO phase signal output

Contract: docs/design/ENGINE_TELEMETRY.md ruling 12 and issue #356.
Append the real per-voice phase output without changing existing IDs/order or control DSP.
The saved qt-lfo-filter patch must load unchanged and render bit-identical control samples.
The additional runtime-patch regression raises the file budget from 8 to 9,
within the workflow's 50% builder allowance. The 200-line budget remains.

```acceptance
tier: T1
size: <= 200 changed lines, <= 9 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ctest --test-dir build/native -R '^abi2_(module|runtime_patch)_tests$' --output-on-failure
  - ci: engine, linux-arm64, web-demo native
stops: [ABI/SDK header edit, changed existing port or param IDs, DSP behaviour change on control]
```

Signed: Codex (telemetry compatibility follow-up), 2026-11-08
