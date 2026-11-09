<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T413 (#413): Acceptance selection ignores quoted and example closing directives

**Purpose.** Prevent `tools/foundation/acceptance.py::select_pr_task` from interpreting Markdown quotations, examples, or code as a PR's actual task-closing directive. Reproduced from #388, whose prose quoted `Closes #384` while explicitly denying that closing directive.

**Scope.** Only the acceptance selector and its existing focused Python tests. Leave `tools/merge_gate.py`, issue-closing semantics, CI/workflow files, and legacy task exceptions unchanged. Do not add dependencies or accept missing/malformed tasks as successful.

**Behavior.**
- A real, visible, top-level line beginning with `Closes #N` (case-insensitive; optional trailing prose/punctuation) selects that issue. First eligible directive wins; multiple real directives retain existing first-directive behavior.
- Ignore inline mentions, Markdown inline code, fenced/indented code, block quotes (including lazy continuations), lists used as examples, and HTML comments. A later real directive must still be selectable.
- The two existing legacy exceptions remain explicitly *unverified*. Missing, ambiguous, or malformed tasks selected by a real directive still fail closed.

**Read first.** `AGENTS.md`, `docs/WORKFLOW.md`, `tools/foundation/acceptance.py`, `tools/foundation/tests/test_accept.py`, and the existing visible-line helper in `tools/merge_gate.py` (read-only). Check current PR bodies that use standalone `Closes #N`, including trailing descriptions.

**Review.** T2 CI acceptance-routing logic; independent Claude/Keel cross-family COMMENT review before merge. No protected CI edit authorized.

```acceptance
tier: T2
size: <= 180 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p 'test_accept*.py'
  - ci: guard, linux-arm64, windows-x64 and sanitizers on the exact PR head
stops: [merge-gate or workflow edit, new dependency, changed issue-closing semantics, legitimate directive form regression, test loosening]
```
Signed: ChatGPT (GPT-6, acceptance-selector builder), 2026-11-08
