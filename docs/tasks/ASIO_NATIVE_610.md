<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #627 — Native ASIO diagnostics and safe preferred-buffer retry

**Keel ruling (supersedes the "unsupported_reason" idea in #610).** A driver that fails `ASIOInit` never appears in `Pa_GetDeviceInfo`, and device IDs must not change, so failed drivers go in a **separate optional list**, not the device list:
1. **Unavailable drivers (Windows only):** in device enumeration (control thread, never audio), read the installed ASIO driver names from `HKLM\SOFTWARE\ASIO` (read-only `RegOpenKeyEx`/`RegEnumKeyEx`; advapi32 is a system library, not a new dependency). Any name with no matching PortAudio ASIO device is reported as `audio.unavailableDrivers: [{hostApi:"ASIO", name, reason:"driver-init-failed"}]`, at most 16 entries with names truncated to 64 bytes, in the `listAudioDevices` reply and the `devices` push. Omitted on Linux and when empty. Not selectable.
2. **Open errors:** `session.cpp` keeps the exception text. On a failed open, `status.audio.error` = an optional string ≤200 bytes built from `Pa_GetErrorText`, plus `Pa_GetLastHostErrorInfo` text for ASIO. It is cleared on the next successful open. Studio shows it in the status bar (covered by #616 or a follow-up there).
3. **Buffer:** on ASIO, if the requested `bufferFrames` isn't accepted, retry **once** at `PaAsio_GetAvailableBufferSizes` preferred size and report the actual value in `status.audio.bufferFrames`. No loops, and no change to the WASAPI/ALSA paths.
4. **Spec:** one additive bullet each for `unavailableDrivers` and `audio.error`. Existing `unsupportedReason` values are unchanged.

**Tests (no real device):** the fake PortAudio/registry seam lists 2 registry drivers with only 1 PortAudio device, giving one `unavailableDrivers` entry. A fake open failure puts the error text in status. A fake buffer rejection retries once and reports the actual size. Golden/conformance expectation updates for these additive fields are part of the change (standing Keel rule) and don't count against the budget.

Budget: Keel authorized 600 changed lines / 10 files for the OSC/MIDI and UTF-8 fixes
([#631 ruling](https://github.com/kalharbi11/wirescape/pull/631#issuecomment-6824198537)).
Assertions, limits and stops are unchanged.

```acceptance
tier: T2
size: <= 600 changed lines, <= 10 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [device id format change, opening a real device in tests, audio-thread work, new third-party dependency, ASIO SDK vendoring]
```
