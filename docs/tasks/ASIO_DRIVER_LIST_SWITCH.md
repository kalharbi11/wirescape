<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #722 Windows ASIO: stable driver list and safe device switch (priority 1)

Read first: #722, `native/io/audio/portaudio_service.cpp` (`Backend`, `devices`, `enumerate`, open/close), `native/sidecar/session.cpp` (`refresh_devices` and its callers, the device-switch path), `native/sidecar/audio_recovery_tests.cpp`, `native/io/audio/DESIGN.md` (ASIO).

- One PortAudio lifetime per process side: enumeration while a stream is open uses the list published when it opened (no fresh `Pa_Initialize`, no `Pa_IsFormatSupported` on ASIO devices other than the open one). A full re-enumeration runs only with no stream open, and never on a timer while ASIO is open.
- Switch: close, stop, quiesce and `Pa_Terminate` the old stream before enumerating or opening the new one. A failed open falls back to the previous device (or null) and reports why, so the result is never silent.
- An ASIO driver that fails to load at enumeration keeps the existing `unavailableDrivers` entry, and its PortAudio host error text goes in a new optional `detail` field (additive; ENGINE_PROTOCOL.md note, Keel approves).
- Tests (device-off, fake backend): refresh during an open ASIO stream doesn't re-initialise; switch A→B→A closes before it opens; a failed open falls back with a reason; an installed-but-failing driver appears with `detail`.
- Realtime path unchanged. No real device opens in tests or CI.

```acceptance
tier: T2
size: <= 450 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - git diff --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [ABI header edit, protocol field renamed or removed, realtime-path change, tolerance loosened]
```

Patch guide impact: none — device handling, not patch capability (the PR says so in its self-review).
Signed: Keel, 2026-11-13
