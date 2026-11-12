<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #653 — Qt cppcheck translation-unit model priority

Read first: #653 (Keel ruling), tools/ci/static_checks.py and
tools/ci/test_static_checks.py.

Classify native/ui/** and build */*_autogen/** source TUs as qt before
checking transitive product headers. Other product TUs stay native.
Keep existing analyzer flags, names, baseline and coverage strictness.
Add regression cases for Qt UI and generated moc source owners with
native headers, plus a native product owner outside those paths.

```acceptance
tier: T2
size: <= 60 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/ci -p test_static_checks.py
  - ci: guard, linux-arm64, sanitizers green
stops: [suppressions, baseline relaxation, skipped units, check-name change]
```
