<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #467: completed task archive and stale-document cleanup

Read #467 and its latest Codex Lead scope ruling. Keep two ordered commits:
resolver/tests/index first, verified closed-issue moves and link repairs second.
Move only tasks whose every issue reference in their first heading is verified
closed. Keep open, mixed and unlinked tasks; keep DEMO_CLASS while #271 is open.
Do not edit legacy/, docs/history/, AGENTS.md or CLAUDE.md.
PLANNER_HANDOFF.md stays unchanged until R5c/#468 because the protected-path
list in AGENTS.md still references it; Codex Lead explicitly deferred that one deletion.

The rename-aware review budget is <=800 substantive changed lines and <=130
files. The acceptance checker deliberately counts deletes/adds without renames:
the parsable ceiling below covers 100 content-preserving task moves plus the
five stale-document deletions and necessary link repairs. Report both
accounts and source/destination content hashes; never weaken the checker.

```acceptance
tier: T1
size: <= 7000 changed lines, <= 240 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_accept.py
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - python tools/dev.py test
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [task deletion instead of move, unverified issue state, edits to legacy/history/agent rules, test weakening]
```

Run python tools/dev.py accept 467 after committing; do not recursively put that
invocation inside its own must_pass list. No devices are opened by this task.

Signed: Codex Lead (Codex, delegated task-archive builder), 2026-11-09 America/New_York.
