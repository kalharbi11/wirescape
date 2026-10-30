<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Owner-authorized studio refinement planning (#139)

Publish the supplied visual reference, a concise source-grounded brief, four
bounded task files/issues, agent ownership and handoff to Claude. Documentation
and reference only; no production/runtime or planner-owned PLAN edits.

```acceptance
tier: T0
size: <= 2000 changed lines, <= 8 files
must_pass:
  - python -c "import sys; sys.path.insert(0,'tools'); from pathlib import Path; from foundation.acceptance import parse; files=list(Path('docs/tasks').glob('STUDIO_*.md')); assert len(files)==5; [parse(p.read_text()) for p in files]; print('5 acceptance blocks valid')"
stops: [runtime change, new asset dependency]
```
