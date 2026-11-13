<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #667 Merge-gate exact-head safety and explicit Markdown-only T0

Read first: #667 ruling, `tools/merge_gate.py`, `.github/workflows/merge_gate.yml`, merge-gate tests.

- On `pull_request_target.synchronize`, disable auto-merge without PR code; every gate denial also revokes it; enabling must match the full checked SHA.
- Permit no-approval auto-merge only with explicit `tier/T0`, no T1/T2 conflict, nonempty `.md`-only diff; protected and milestone PRs still require exact-head signed approval.
- Prove positive/negative decisions and synchronize-only behavior without new tokens, permissions, or weakening required checks.

```acceptance
tier: T2
size: <= 120 changed lines, <= 4 files
must_pass:
  - python -m unittest tools.foundation.tests.test_merge_gate
  - actionlint
  - ci: guard, linux-arm64, sanitizers green
stops: [running PR code in pull_request_target, new token or permission, weakening exact-head approval, required-check change]
```

Signed: builder-chat-2 (builder), 2026-11-12
