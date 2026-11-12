<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #637 — Hosted Windows wake-only retry
Keel: once only after a wake-only overrun on hosted Windows CI; log both outcomes
and require a clean retry. No render/fault/other retries, non-Windows retries or limit changes.
```acceptance
tier: T2
size: <= 80 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [changing any overrun limit, retrying render overruns, retry outside hosted Windows]
```
