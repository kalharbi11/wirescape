<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Senior low-Claude handoff and gate task discovery (#714)

Owner: Codex Lead. Docs only; no CI, product or policy implementation changes.
Refresh STATE with completed senior merges, exact evidence and current assignments.
Add the missing task for issue701 with Keel's existing acceptance unchanged.
Record low-Claude pauses without reassigning Claude builders or waiving T2 review.
Keep the owner's pinned candidate distinct from later main verification.

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; p=Path('docs/planner/STATE.md'); assert len(p.read_text(encoding='utf-8').splitlines()) <= 60"
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - python -m unittest discover -s tools/foundation/tests
  - git diff --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [product change, CI policy change, protected workflow edit, acceptance weakening]
```

Patch guide impact: none — planner and task documentation.
Signed: Codex Lead (GPT-6), 2026-11-12
