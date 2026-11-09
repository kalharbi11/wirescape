<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #466: Qt-free product quickstarts and current evidence

Rewrite only README.md, native/README.md, CONTRIBUTING.md and docs/STATUS.md.
Describe Electron + Studio + wirescape-engine, explicit headless source builds,
device-off launches, owner-only hardware consent and current evidence. Keep the
Windows first-session result distinct from pending final runtime/Pi validation.
Link the merged Windows setup guide; an accepted combined install SHA remains pending.
Historical evidence remains in Git history; no code, archived files or agent
rules change. Codex Lead authorized this task path and a narrow factual
docs/planner/STATE.md refresh on #466; the docs-only file budget is six.

```acceptance
tier: T0
size: <= 400 changed lines, <= 6 files
must_pass:
  - python -c "import pathlib,sys; bad=[p for p in ['README.md','native/README.md','CONTRIBUTING.md'] if 'Qt 6' in pathlib.Path(p).read_text() or 'qt-creator' in pathlib.Path(p).read_text().lower()]; sys.exit(1 if bad else 0)"
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [code change, unsupported hardware or release claim, protected path outside this named task or STATE]
```

Signed: Codex Lead (Codex, delegated docs builder), 2026-11-09 America/New_York.
