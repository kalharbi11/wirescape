<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #636 — Restore scheduled CI verification of main

Keel-approved follow-up after #635: every 30 minutes or on manual dispatch,
start missing CI/Web/Web demo workflow runs for the current main SHA, at most once
per workflow and SHA. Cache save on main includes dispatch, not pull requests.

```acceptance
tier: T2
size: <= 120 changed lines, <= 4 files
must_pass:
  - actionlint
  - python -m unittest discover -s tools/foundation/tests -p test_main_verify_dispatch.py
  - ci: guard, linux-arm64, sanitizers green
stops: [merge gate logic change, required-check change, PAT or new secret, dispatch loops]
```
