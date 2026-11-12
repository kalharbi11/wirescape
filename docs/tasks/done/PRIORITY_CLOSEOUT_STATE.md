<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Priority closeout state (#682)

Read first: AGENTS.md, docs/planner/LEADS.md, board #473 BODY and latest
source-head handoffs. Persist the senior run's priority and evidence snapshot.
Keep STATE at most60lines, names/ownership current, merged rows complete,
acceptance/running work distinct, and model-family/protected/hardware gates.
Only STATE, this brief and its task-index entry change; no product/CI/spec/plan edit.
Patch guide impact: none — coordination docs only.

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; s=Path('docs/planner/STATE.md').read_text(); assert len(s.splitlines()) <= 60; assert all(x in s for x in ('#672', '#675', '#631', '#679', 'Claude', 'owner'))"
  - python -m unittest tools.foundation.tests.test_doc_links
  - python -m unittest discover -s tools/foundation/tests
  - bash -lc 'git diff --check'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [runtime edit, CI policy edit, plan change, evidence fabrication, test weakening]
```
