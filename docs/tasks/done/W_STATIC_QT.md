<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Static Qt compile context (#136)

Review routing: T2-Claude (label-only CI tooling).

Authorized by #136: provide existing Qt development dependencies and a full
compile database; resolve changed headers/X-macro includes through compiler
recorded dependencies, preserving line filters and fail-closed ownership.
Cppcheck uses the owning compile commands and its Qt model. No suppression
of findings, check removal, new library dependency or runtime behavior change.

```acceptance
tier: T2
size: <= 250 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/ci -v
  - python -m pip install ruff==0.14.1
  - python -m ruff check tools/ci/static_checks.py tools/ci/test_static_checks.py
  - actionlint
  - ci: static job green on this PR
  - ci: static re-run green on #79 after merging main
stops: [check weakened, missing compiler owner accepted]
```
