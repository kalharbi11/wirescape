<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows — isolated Electron source build (W1)

**Status: source preparation; NOT an accepted reinstall or downloadable installer.**
Follow #436 and #463. Only after #463 merges and the exact source revision passes
native/Windows, Studio, Electron and required CI can an exact accepted SHA
be entered below. The **#463 runtime merge commit alone predates this W1
script**: a runnable owner checkout must include both #463 and this W1 PR.
Codex Lead confirms the combined source SHA after validation, before owner use. None is yet accepted. CI may validate its own immutable candidate SHA with
`-Ref`; a candidate CI run does not declare an owner-accepted installation.

## Preserve the old installation
Use a **separate clean Git checkout**, preferably
`$HOME\Documents\code\wirescape`. Do not delete/move the old checkout, old
settings, saved patches, shortcuts, system Qt, or another app's PortAudio.
Do not copy binaries, node_modules, .tools, CMake caches or PATH entries from
the old checkout. This is the app with the Studio renderer and
`wirescape-engine`, not the old Qt Studio.

## Requirements (owner-managed)
Windows x64 and Developer PowerShell for VS 2022 with MSVC x64 C++ workload and
Windows SDK; Git, Python 3.12 x64 with `py`, Node.js 22 LTS x64 and npm, CMake,
Ninja; GitHub and PortAudio upstream reachability for the actual build.
Install missing tools separately using their official sources; the script
never runs winget, changes User/Machine PATH or elevates privileges.
After installation, reopen Developer PowerShell.

## Check, build, then test
Run these commands from the **fresh** checkout, never an old app directory:

```powershell
.\tools\windows\setup.ps1 -CheckOnly
# After #463 and W1 merge, and their combined SHA is formally accepted:
$AcceptedSha = '<reviewed full 40-character merge SHA>'
git fetch origin
if ($LASTEXITCODE -ne 0) { throw 'Fetch failed' }
git switch --detach $AcceptedSha
if ($LASTEXITCODE -ne 0) { throw 'Checkout failed' }
.\tools\windows\setup.ps1 -CheckOnly -Ref $AcceptedSha
.\tools\windows\setup.ps1 -Ref $AcceptedSha -NoLaunch
# Only after that succeeds: device-off Electron launch/probe
.\tools\windows\setup.ps1 -Ref $AcceptedSha
```

`-CheckOnly` inspects the current checkout and tools but **does not** fetch,
write reports, install, configure, build, or contact the network. The
non-check path rejects dirty and mismatched source/cache, isolates only the
current PowerShell process environment, builds Qt-free native + Studio +
Desktop, and stages, loads without initializing devices, and resolves the required exports
of a fresh x64 PortAudio DLL from the same pinned
`147dd722548358763a8b649b3e4b41dfffbcfbb6` commit as CI.
Build evidence is local under `build_reports/windows-setup/summary.txt` and
`protocol-tests.log`; PortAudio/caches stay in `.tools/windows-setup/`.
The smoke uses a unique temporary profile, removed after shutdown, and stores
failed-test artifacts under the diagnostic directory. Do not upload logs
containing machine-specific paths.

Tests must actually execute, not be skipped. After the native suites pass,
the optional Electron probe launches with `--null-audio`, temporary native
`--settings`, and explicit isolated Electron `userData` and `sessionData`.
It runs the Windows Playwright real-Electron harness, requires the visible
window/UI-ready state and native command/patch roundtrip, preserves opaque patch
data, closes Electron gracefully, and verifies its native child has exited.
Both redirected output paths and their existing ancestors are refused before
writing. Windows CI also executes these checks and filesystem-junction regressions.
Until an actual Windows acceptance report passes, W1 remains blocked.

## Owner launch and limitations
Once all W1/CI gates pass, the owner may intentionally test real audio/MIDI;
the automatic script never supplies device-confirmation flags. The legacy
`tools/windows/Wirescape.cmd` explicitly opts into devices and should not
be used for CI or a device-off smoke. Repeated device names now include their
host API (subject to #463 merge). Shared WASAPI must run at the device's
own rate (48 kHz in the owner's test); another DAW such as Ableton must
release exclusive or otherwise conflicting access before launching.

Failures: preserve checkout-local logs and actual SHA, do not reuse old DLLs,
change test thresholds, unskip results or force a stale cache. A failed
protocol, absent Electron hello, missing UI evidence, or unmerged #463 is
**BLOCKED**, never a verified clean installation. W2 packaging/release and
Pi installation remain separate #436 work.

Signed: ChatGPT (GPT-6 Windows W1 source-setup builder), 2026-11-09
