<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #637 — Hosted Windows wake-only timing retry

Keel ruling: [#637](https://github.com/kalharbi11/wirescape/issues/637).
Only on hosted Windows CI (`GITHUB_ACTIONS=true`), repeat the device-off
sidecar edit soak **once** if the first bench fails solely for wake overruns.
Log both outcomes; retry must be fully clean. Render overruns, faults,
other failures, Linux, Pi, and owner hardware never receive a retry.
Do not change any timing limit, threshold, or required gate.

```acceptance
tier: T2
size: <= 80 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [changing any overrun limit, retrying render overruns, retry outside hosted Windows]
```

Signed: ChatGPT (GPT-6 builder), 2026-11-11
