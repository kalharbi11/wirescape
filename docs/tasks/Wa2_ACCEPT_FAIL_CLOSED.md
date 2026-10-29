<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-a2: Fail closed on missing PR acceptance (#114)

**Scope:** `Closes #N` always selects exactly one parseable task. Missing,
ambiguous, malformed or unreadable tasks produce an actionable **not verified**
error and nonzero exit in CI. The selection logic lives in
`tools/foundation/acceptance.py` and the only authorized workflow edit is
`.github/workflows/ci.yml`.

**Legacy exceptions:** exactly **#45** (user's J9 session) and **#64**
(deferred laptop measurement). They are explicitly skipped by this optional
job and logged as **legacy exception — not verified**, never as a successful
task acceptance. PRs without `Closes #N` retain the optional no-task behavior.

```acceptance
tier: T0
budget: 1 ChatGPT CI iteration
size: <= 220 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_accept*.py"
  - ci: missing or ambiguous Closes task and invalid block fail non-zero with not verified
  - ci: valid Closes task runs real acceptance; linux-arm64, windows-x64, guard green
stops: [new dependency, threshold change, acceptance weakening, any other protected path]
```
