<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #380 Static web compile ownership and telemetry timing

**Issue:** [#380](https://github.com/kalharbi11/wirescape/issues/380)  
**Tier:** T2  
**Budget:** The implementation allowance remains the issue’s original <= 120 changed lines across <= 4 implementation files. The pre-existing docs/planner/STATE.md lead vehicle and this acceptance task file are outside that implementation allowance. For acceptance accounting across the PR, allow <= 160 changed lines across <= 6 files (including the lead vehicle and this task file); do not weaken source or analyzer checks.

## Read first

- Issue #380 and its Keel ruling.
- `.github/workflows/static.yml`
- `.github/workflows/web-demo.yml`
- `tools/ci/static_checks.py`
- `tools/ci/test_static_checks.py`

## Acceptance

```acceptance
tier: T2
size: <= 160 changed lines, <= 6 files
must_pass:
  - python -m unittest tools/ci/test_static_checks.py
  - ci: static job reports compiled owners for native/web/*.cpp on a PR that changes them
  - ci: web demo uploads the verbose native telemetry timing artifact
stops: [excluding files, skipping a missing build dir, loosening any check or threshold]
```
