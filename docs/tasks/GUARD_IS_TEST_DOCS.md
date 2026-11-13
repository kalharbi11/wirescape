<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #719 Guard: Markdown docs named "*test*" are not tests

Read first: #719, `tools/ci/guard.py` (`is_test`, the D/R check), `tools/ci/test_guard.py` (the guard regression suite; run by `guard.yml`), #708's failed guard run.

- Today `is_test()` matches "test" anywhere in a stem, so moving `docs/tasks/OWNER_TEST_SESSION_271.md` into `done/` needs `allow-test-removal`.
- Change: a `.md` file under `docs/` is not a test. Everything else keeps the current rule, including any file under a `test*`/`tests` directory, and `.py/.ts/.tsx/.cpp/.h/.json` test sources and fixtures.
- Tests (additive, in `tools/ci/test_guard.py`): a `docs/tasks/*TEST*.md` rename passes without the label; deleting `tools/ci/test_guard.py`, `native/tests/x.cpp`, `apps/studio/tests/a.spec.ts` or `docs/tests/x.md` still fails without it.
- `tools/ci/**` is label-only: Keel applies `allow-protected`. T2: one review from the Claude family, plus Keel's CI-policy approval.

```acceptance
tier: T2
size: <= 80 changed lines, <= 2 files
must_pass:
  - python -m unittest discover -s tools/ci -p test_guard.py -v
  - python -m unittest discover -s tools/foundation/tests
  - git diff --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [any existing guard assertion weakened, test detection narrowed outside docs/**.md, workflow edit]
```

Patch guide impact: none — CI guard only.
Signed: Keel, 2026-11-12
