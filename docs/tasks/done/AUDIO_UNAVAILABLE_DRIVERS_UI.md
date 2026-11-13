<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #723 Studio: show ASIO drivers that are installed but failed to start (priority 1)

Read first: #723, `native/sidecar/session.cpp` (`unavailableDrivers` in `listAudioDevices` and status), `apps/studio/src/engine/RealEngine.ts`, the Audio settings panel, `apps/studio/src/engine/FakeEngine.ts`, `apps/studio/AGENTS.md`.

- Audio settings lists each `unavailableDrivers` entry under its host API as disabled, e.g. "FlexASIO: installed, but failed to start". Show `detail` when present (#722 adds it). Add one hint line: "Close other audio apps using this driver, check its settings, then Refresh."
- Unknown fields are ignored; the list renders as before when the array is absent.
- FakeEngine gains an optional `unavailableDrivers` fixture; Playwright covers present/absent.

```acceptance
tier: T1
size: <= 200 changed lines, <= 5 files
must_pass:
  - cd apps/studio && npm ci && npm test
  - git diff --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [protocol change, new dependency]
```

Patch guide impact: none — settings UI only.
Signed: Keel, 2026-11-13
