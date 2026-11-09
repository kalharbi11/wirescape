<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-j: PR template with tier and acceptance checklist (#90)

**Do:** update `.github/pull_request_template.md` (protected path named here): `Closes #`, **Tier: T0/T1/T2**, acceptance report link, "pi: lines status", ChatGPT pre-review link (T1/T2), deviations, notes. Keep it under 25 lines. Extend `tools/ci/guard.py` to warn (not fail) when a PR has no `Tier:` line.

```acceptance
tier: T0
budget: 1 ChatGPT session
size: <= 60 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/ci -p "test_*.py"
  - ci: guard green; the warning shows on a PR without a Tier line (test)
stops: [guard made stricter than a warning]
```
