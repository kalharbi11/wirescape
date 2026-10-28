<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-i: pre-commit hooks mirroring the guard (#89)

**Do:** `.pre-commit-config.yaml` with local hooks only (no remote hook repos = no new dependency beyond `pre-commit` itself, which is optional for contributors): SPDX header on new files, no binaries/generated files (same list as `tools/ci/guard.py`), protected-path warning, clang-format on changed lines (after W-c). Hooks call small functions in `tools/ci/guard.py` so the rules live in one place. Document in `CONTRIBUTING.md` (one paragraph).

```acceptance
tier: T0
budget: 1 Codex session
size: <= 200 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/ci -p "test_*.py"
  - pre-commit run --all-files        # clean on main
  - ci: linux-arm64, windows-x64, guard green
stops: [guard rule weakened, new dependency other than pre-commit]
```
