<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #667 Merge-gate exact-head safety and explicit Markdown-only T0

Read first: issue #667 (Keel's binding ruling), `tools/merge_gate.py`,
`.github/workflows/merge_gate.yml`, and current merge-gate unit tests.

- On `pull_request_target.synchronize`, disable auto-merge without running
  incoming PR code or the approval script. Ignore only already-disabled errors.
- Permit no-approval auto-merge only with `tier/T0`, no conflicting T1/T2 tier,
  and a nonempty diff containing exclusively `.md` files. Protected paths and
  milestone reviews still require exact-head owner-authored signed approval.
- Prove positive/negative decisions and the synchronize-only workflow path
  without changing required checks, workflow permissions or tokens.

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
