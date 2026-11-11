<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Advisory acceptance command/fence preflight (#574)

T1 helper tooling only. Read #574, #568's malformed acceptance-command review,
`tools/foundation/acceptance.py` and its `tests/test_accept.py` tests.

Add standalone, **advisory** CLI in `tools/foundation/` for explicitly named
Markdown task paths. Check exactly one fenced `acceptance` block, parse via
the existing parser, and diagnose local shell syntax/unterminated quotes
without executing any task command. Add fixture tests for valid nested
Chromium commands, invalid unmatched quote, duplicated fence and mixed
external `ci:`/`pi-run:` fields. Do not edit the real acceptance parser,
workflow, controls, tests or thresholds. Integrating a policy gate is a
separate Keel T2 decision. Needs independent T1 review.
Patch guide impact: none — advisory tooling only.

```acceptance
tier: T1
size: <= 220 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_task_acceptance_audit.py
  - python -m unittest discover -s tools/foundation/tests -p test_accept.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [acceptance parser edit, CI control or workflow edit, test execution by auditor, new policy or threshold]
```
