<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #754 R5c: archive `legacy/` and `docs/history/`, refresh read maps

Read first: `docs/planner/CLEANUP.md` (R5c), PR #716, tag `archive/qt-final` (`e6e927c`).

- Delete `legacy/**` and `docs/history/**` (both preserved at `archive/qt-final`). Nothing outside those two folders is deleted.
- Update the docs that point at them (AGENTS.md read map and label-only list, ARCHITECTURE, STATUS, PI_* docs, native/README, the M1 review link to the archived SHA).
- `allow-test-removal` covers only the 16 test-like files inside the two archived folders (Keel, 2026-11-14). `allow-protected` covers AGENTS.md.
- `tools/ci/guard.py` and `.github/CODEOWNERS` still name the archived paths. Leave them unchanged here (that's T2 CI policy, a later follow-up).

```acceptance
tier: T0
size: n/a
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - python -m unittest discover -s tools/foundation/tests
  - bash -c 'git diff --check origin/main...HEAD'
  - bash -c 'test ! -e legacy && test ! -e docs/history'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [deletion outside legacy/ or docs/history/, native or test-code change, guard/CODEOWNERS edit]
```

Patch guide impact: none — archive and docs only.
Signed: Keel, 2026-11-14
