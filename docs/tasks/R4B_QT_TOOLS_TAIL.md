<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #718 R4b: retire Qt from the Pi probe, Pi preflight, Windows setup and cloud setup

Read first: Keel ruling on #661 (2026-11-12), `docs/planner/CLEANUP.md` (R4/R4b rows), `tools/pi_ui_probe.py`, `tools/pi/preflight.py`, `tools/windows/setup.ps1`, `tools/cloud/`.

- Split from R4 #469 so #481 stays in budget. Starts after #481 merges (R3b #713 has already removed Qt from the build).
- `tools/pi_ui_probe.py`: drop the Qt UI build (`WIRESCAPE_BUILD_UI=ON`, offscreen Qt env). If nothing non-Qt is left, delete it and its test.
- `tools/pi/preflight.py`: drop the Qt override/cache checks. Keep every non-Qt preflight check and its test.
- `tools/windows/setup.ps1`: drop the Qt cache/quarantine logic. Keep Windows PowerShell 5.1 compatibility (#703).
- `tools/cloud/*`: drop any Qt install or env. The cloud setup must still build `--headless`.
- A table in the PR names each removed test case and the Qt-only behaviour it covered. Keel applies `allow-test-removal` for that table only.
- Devices stay off; no new dependency; required check names unchanged.

```acceptance
tier: T1
size: <= 600 changed lines, <= 10 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests
  - git grep -nE "WIRESCAPE_BUILD_UI|QT_QPA_PLATFORM|Qt6_DIR|WIRESCAPE_QT_DIR" -- tools/pi_ui_probe.py tools/pi tools/windows tools/cloud ; test $? -eq 1
  - git diff --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [non-Qt test removed, new dependency, required check renamed, ABI or protocol change]
```

Patch guide impact: none — developer tooling only.
Signed: Keel, 2026-11-12
