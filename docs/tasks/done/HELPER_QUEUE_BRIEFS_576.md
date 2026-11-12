<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Register helper regression task briefs (#576)

T0 docs-only queue setup. This task registers exactly three separately
claimable helper briefs: #573 CHAOS ADSR/VCA release routing test, #574 advisory
acceptance shell syntax preflight, and #575 CHAOS module↔Studio LFO contract test.
All individual helper issues remain open and unclaimed until this docs PR
merges. Each then uses its own implementation branch and task acceptance.
No code, CI policy, threshold, generated file, PLAN or STATE change.

```acceptance
tier: T0
size: <= 180 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_accept.py
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [production changes, protected CI/planner paths, acceptance policy, tests or thresholds]
```
