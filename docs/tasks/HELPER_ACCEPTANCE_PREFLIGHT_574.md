<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #574: advisory preflight for malformed task acceptance commands

Issue #574, T1 helper. New advisory CLI `tools/foundation/acceptance_preflight.py`
(`python tools/foundation/acceptance_preflight.py docs/tasks/X.md`) that checks for
exactly one fenced `acceptance` block, reuses `acceptance.parse`, and reports
unterminated quotes / `bash -n` errors with the command index. It never runs the
listed commands and adds no policy; it is not wired into CI (a separate Keel T2 decision).
`acceptance.py`, `tools/ci/**`, workflows and existing task files are untouched.
Patch guide impact: none, tooling only.

```acceptance
tier: T1
size: <= 220 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_acceptance_preflight.py
  - python -m unittest discover -s tools/foundation/tests -p test_task_acceptance_audit.py
  - python -m unittest discover -s tools/foundation/tests -p test_accept.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [editing acceptance parser, workflows, CI policy, executing listed acceptance commands, changing test selection]
```

Signed: Helper (Claude, build), 2026-11-11
