<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #437: R0 plan and cleanup roadmap reconciliation

Land Compass D15–D19 and the cleanup issue map without runtime, ABI or test changes.
The future Windows setup guide is a planned deliverable of #436, not a current
runbook. Preserve the owner's first Windows result and board #473 as the claim source.
Keel authorized PLAN/STATE edits with the existing allow-protected label on #474.
Codex Lead named this task path on #437.

```acceptance
tier: T0
size: <= 400 changed lines, <= 5 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - python -c "from pathlib import Path; assert len(Path('docs/planner/STATE.md').read_text().splitlines()) < 60; assert sum(l.startswith('## Lane ') for l in Path('docs/tasks/PR_CLOSEOUT_2026_11_09.md').read_text().splitlines()) == 10; t=Path('docs/planner/CLEANUP.md').read_text(); assert all(str(n) in t for n in range(464,471))"
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [runtime change, planner decision change, test weakening]
```

Signed: Codex Lead (Codex), 2026-11-09 America/New_York.
