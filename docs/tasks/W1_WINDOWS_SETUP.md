<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #436 W1 — one Windows Electron source-setup path

Claim: board #473 row 4. Keel's 2026-11-09 ruling on #436 supersedes the
older install scope. This is a source build, **not** a downloadable installer.
The accepted build SHA is the eventual merge commit of #463, not a guess.

## Read first
- AGENTS.md, docs/WORKFLOW.md, #436 latest Keel ruling and handoffs.
- docs/WINDOWS_DEMO_UPDATE.md, tools/windows/Wirescape.cmd
- tools/foundation/helpers.py, apps/desktop/main.ts, apps/desktop/package.json
- The corrected (now-closed) #443 guide at source commit ce11b212.

## Deliver
- docs/WINDOWS_SETUP.md: one truthful owner-facing Qt-free Windows source
  path; independent checkout, preserved user data, process-only isolation,
  checked prerequisites and source SHA, pinned PortAudio and device-off
  verification before any owner-authorized real device launch.
- tools/windows/setup.ps1: read-only -CheckOnly, explicit full SHA,
  preflight for Win x64 + VS 2022 x64 C++ and SDK, Python 3.12, Node 22,
  Git/CMake/Ninja/npm; fail closed on dirty/mismatched checkout/cache.
  On explicit build only, compile native and both npm apps, stage and verify
  pinned PortAudio, execute real non-skipped protocol tests, and attempt
  a bounded device-off Electron smoke in isolated app and native settings.
- Owner only handles any prerequisite installation and real audio/MIDI launch.
  No automatic winget/apt/sudo, old-install deletion, release, user-wide
  environment edits, service/shortcut installs or device-confirmation flags.
- Keep shell/paths safe with spaces and retain source/stage evidence in
  ignored checkout-local files. A smoke skip or missing window/hello is
  blocked, not passed.
- Protected .github/workflows changes require separate Keel/Compass
  authorization; do not add a CI job before that.

## Review and open gates
Independent T1 COMMENT, all required exact-head checks, and executed Windows
smoke. #463 must merge before an accepted source SHA or owner readiness is set.
A PR referencing #436 must not auto-close the umbrella issue, which
continues to track W2/P1 separately.

Signed: ChatGPT (GPT-6 Windows W1 source-setup builder), 2026-11-09

```acceptance
tier: T1
size: <= 650 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_windows_setup.py
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on final head
  - ci: Windows PowerShell syntax, setup.ps1 -CheckOnly and native protocol tests execute without SKIP
stops: [new dependency, protected CI edit, silent install, device use, false-positive smoke, test relaxation]
```
