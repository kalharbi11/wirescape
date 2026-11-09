<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-a: `python tools/dev.py accept <issue#>` (#85)

**Why:** "done" is decided by a script, not by AI reading. Format: `docs/WORKFLOW.md` §4.

**Do:**
1. `accept <issue#>`: find the task file whose title line names `#<issue#>` (or `--file`), parse the last fenced `acceptance` block (small hand parser; no PyYAML dependency), run each `must_pass` line that is a shell command (`python …`, `ctest …`, `bash …`) with a timeout, skip-and-list lines prefixed `pi:`, `ci:`, `user:`, `chatgpt-pre-review:` as "external". Check `size` against `git diff --numstat origin/main...HEAD`.
2. Write `build_reports/accept-<issue>.json` and a Markdown summary; exit non-zero on any failure. `--json` prints only the summary.
3. CI: a job step that runs `accept` when the PR body has `Closes #N` and the task file has a block; it posts the Markdown to `$GITHUB_STEP_SUMMARY` (non-required at first). Protected path: this issue names `.github/workflows/ci.yml`.
4. Unit tests with fixture task files (pass, fail, external lines, size overflow, missing block).

```acceptance
tier: T0
budget: 1 Codex session
size: <= 450 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_accept*.py"
  - python tools/dev.py accept --file docs/tasks/J8_AUTHOR_GUIDE.md        # runs green on main
  - ci: linux-arm64, windows-x64, guard green; the accept step summary appears on this PR
stops: [new dependency, threshold change]
```
