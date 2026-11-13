<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Completed priority lanes (#697)

Read AGENTS.md, docs/planner/LEADS.md and board#473. Record actual OSC/Quad/ASIO
software completion, current other-PR blockers and candidate→owner-test→tag order.
Only STATE, this brief and its index change. Keep live SEND THESE on#473.
Patch guide impact: none — coordination docs only.

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; s=Path('docs/planner/STATE.md').read_text(); assert len(s.splitlines()) <= 60; assert all(x in s for x in ('82bab6c', '#631', '#593', '#696', 'owner', 'Claude'))"
  - python -m unittest tools.foundation.tests.test_doc_links
  - python -m unittest discover -s tools/foundation/tests
  - bash -lc 'git diff --check'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [runtime edit, CI-policy edit, plan change, evidence fabrication, test weakening]
```
