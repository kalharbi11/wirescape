<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Merge gate: require exactly one tier (#701)

Owner: gpt-builder3. Existing PR #712; continue it, no duplicate implementation.
Read first: issue #701, tools/merge_gate.py, tools/ci/guard.py and their tests.
Keel's existing ruling governs; this task only makes that acceptance discoverable.

- Before every approval branch, refuse missing or multiple tier/T0,T1,T2 labels.
- Guard fails with an actionable message; body Tier text remains advisory.
- Preserve explicit T0 Markdown-only handling and signed exact-head T1/T2 approval.
- Positive approval fixtures carry exactly one tier, including milestone cases.
  Missing/conflicting-tier negatives must remain refused even with a signature.
- Add no required-check rename, token, permission or approval-format change.
- Merge current main normally after the docs packet lands; no force push.
- tools/ci paths require allow-protected; final independent Claude T2 and Keel
  CI-policy approval remain required. Claude unavailability does not waive them.

Acceptance copied unchanged from issue #701:

```acceptance
tier: T2
size: <= 80 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_merge_gate.py
  - python -m unittest discover -s tools/ci -p test_guard.py
  - ci: guard, linux-arm64, sanitizers green
stops: [required-check rename, new secret or token, approval-format change]
```

Patch guide impact: none — merge control only.
Signed: Codex Lead (GPT-6), 2026-11-12
