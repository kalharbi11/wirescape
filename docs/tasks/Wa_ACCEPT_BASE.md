<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Acceptance base for stacked PRs (#116)

Planner authorization: issue #29, 2026-10-29 and WORKFLOW section 3b.
Add `accept --base <ref>`; default remains `origin/main`. Resolve a commit
before measuring the merge-base diff and record both ref and commit in the
report. Invalid bases fail without running task commands. Do not change limits
or external-evidence rules. After a parent merges, acceptance runs against main.

```acceptance
tier: T0
budget: 1 Codex session
size: <= 180 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_accept*.py"
  - ci: linux-arm64, windows-x64, guard green
stops: [new dependency, relaxed limits]
```
