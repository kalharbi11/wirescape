<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #653 — Qt UI and generated moc translation units use the cppcheck Qt model

Regression from #621: cppcheck's product-header precedence selects the native
model for Qt UI and moc translation units that transitively include engine
headers, producing Qt keyword diagnostics unrelated to changed first-party code.

**Scope:** In `tools/ci/static_checks.py::cppcheck_model`, select `qt`
first when the compiled TU is under `native/ui/` or a generated
`*_autogen/` directory. Preserve native product-header precedence for every
other TU. Do not change suppressions, baselines, flags, or check names.

**Tests:** Extend existing synthetic compile-database/Ninja dependency cases
with `native/ui/x.cpp` (including `native/engine2/common.h`) and
`build/static/ui/foo_autogen/moc_x.cpp`; both must select Qt. Existing
non-Qt product ownership and Qt project-command assertions remain.

**Authorization:** #653 names the protected path `tools/ci/static_checks.py`
and bears `allow-protected`; request that label on the PR from Keel.

```acceptance
tier: T2
size: <= 60 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/ci -p test_static_checks.py
  - ci: guard, linux-arm64, sanitizers green
stops: [suppressions, baseline relaxation, skipped units, check-name change]
```
