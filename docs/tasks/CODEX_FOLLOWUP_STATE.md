<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Codex requested followup state (#694)

Read AGENTS.md, docs/planner/LEADS.md, PLAN Now/Decisions and board#473.
Persist this distinct owner-requested followup's actual software merges, open
head-specific blockers, named agent briefs and accepted candidate/test/tag order.
Only STATE, this brief and its index entry change; no product, CI, spec or plan.
Patch guide impact: none — coordination docs only.

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; s=Path('docs/planner/STATE.md').read_text(); assert len(s.splitlines()) <= 60; assert all(x in s for x in ('#684', '#631', '#691', '#690', '#693', 'Claude', 'owner'))"
  - python -m unittest tools.foundation.tests.test_doc_links
  - python -m unittest discover -s tools/foundation/tests
  - bash -lc 'git diff --check'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [runtime edit, CI-policy edit, plan change, evidence fabrication, test weakening]
```
