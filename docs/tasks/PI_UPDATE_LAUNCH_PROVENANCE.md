<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #752 Pi update-and-build leaves a launchable Electron build

Read first: `tools/pi/update-and-build.sh`, `tools/dev.py` build/launch dispatch, `tools/foundation/` build provenance helpers and existing Pi update tests, `docs/INSTALL.md`.

- Preserve SHA/clean-source/toolchain checks and device-off defaults. Make the updater's successful native + Electron build record the same configuration provenance that `python3 tools/dev.py launch` verifies. Prefer the existing `dev.py build --config Release` entry point; do not invent a second provenance format or bypass launch checks.
- Remove the redundant manual rebuild workaround in INSTALL only once the updater supplies it. Keep physical launch/audio/MIDI steps owner-only.
- Add device-free mocked subprocess/temp-directory regression proving a successful updater uses the supported build path, propagates build failure, and does not open devices. Existing provenance refusals remain strict.
- Scope: updater, focused tests, INSTALL; no CI policy, ABI or engine behavior changes.

```acceptance
tier: T1
size: <= 250 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [provenance validation bypass, real devices or SSH, new dependency, CI policy change]
```

Owner Pi launch remains a separate hardware confirmation; CI must not start it.
Patch guide impact: none — installation/build provenance only.
Signed: Codex Lead, 2026-11-14
