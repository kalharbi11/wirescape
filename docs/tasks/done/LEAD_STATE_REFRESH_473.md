<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Lead queue reconciliation (#473)

Read first: AGENTS.md, docs/planner/LEADS.md, board #473 newest SEND THESE.
Update docs/planner/STATE.md once with current review blockers and evidence.
Authority: Codex Lead; named paths authorized on #473. Keep STATE at most 60 lines.
Preserve claims, hardware evidence boundaries and protected approval authority.
Patch guide impact: none — coordination documents only.

```acceptance
tier: T0
size: <= 180 changed lines, <= 2 files
must_pass:
  - python -m unittest tools.foundation.tests.test_doc_links
  - python -m unittest discover -s tools/foundation/tests
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [product change, workflow change, evidence fabrication]
```
