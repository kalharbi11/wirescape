<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #637 — Hosted Windows wake-only retry
Keel: retry once only for wake-only overruns on hosted Windows CI, with both
outcomes logged and a fully clean retry; never retry render/fault/other failures,
Linux, Pi, or owner hardware. Preserve every overrun limit.
```acceptance
tier: T2
size: <= 80 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [changing any overrun limit, retrying render overruns, retry outside hosted Windows]
```
