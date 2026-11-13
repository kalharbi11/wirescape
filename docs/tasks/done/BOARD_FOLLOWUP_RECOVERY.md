<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #696 — Board follow-ups: failures stay visible, never skip main verification

Owner: Keel (builder and CI-policy owner). Review: GPT family (Codex Lead or `gpt-reviewer`), T2.
Folds in #679's sound parts; #679's Guard `workflow_run` trigger is dropped (gpt-reviewer High: not a post-merge signal).

**Problem.** `board.py --followups` ran three steps in one sequence: close linked issues, clear
`status/*` from closed items, then dispatch `main_verify`. The label step got HTTP 403 on closed PRs
(the job had `pull-requests: read`), so main verification never ran.

## Steps
1. `tools/board.py` `followups()`: three independent steps, most important first:
   `_verify_main` → `_close_linked` → `_clear_closed_status`. A failing step is printed and never
   skips a later one; any failure makes the job exit 1. 404 on a label delete stays idempotent.
2. `board.yml` `followups` job: `pull-requests: write` (PR labels). The board also rebuilds on
   `issues.edited`.
3. `pending_closes()` ignores `Closes #N` inside inline code and fenced blocks (#681 Medium).
4. From #679: the merge gate disables auto-merge only when it is armed (no red runs for a "no");
   guard reads `Protected path named:` only from owner/member/collaborator issues; Static 40 min.

Post-merge wakeup (the #679 and #702 High): `post_merge.yml` runs after every completed CI, Guard
and Merge gate run (GitHub arms and fires auto-merge right after one of these). `board.py
--watch-merges 12` waits, bounded to 12 minutes, while any open PR has auto-merge armed, and verifies
main once a merge lands and always once before it exits. A newer watch replaces an older one
(concurrency group). Regressions cover a merge that lands after the Board run, no armed PRs, and an
auto-merge that never lands (bounded).

```acceptance
tier: T2
size: <= 300 changed lines, <= 10 files
must_pass:
  - actionlint
  - python -m unittest discover -s tools/foundation/tests -p test_board.py
  - python -m unittest discover -s tools/foundation/tests -p test_merge_gate.py
  - python -m unittest discover -s tools/ci -p test_guard.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [required-check rename, new secret or token, approval-format change]
```

Patch guide impact: none — CI/board tooling only.

Signed: Keel, 2026-11-12
