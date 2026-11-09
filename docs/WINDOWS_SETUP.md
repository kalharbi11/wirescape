<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows — isolated Electron source build (W1)

**Status: source preparation; NOT an accepted reinstall or downloadable installer.**
Follow #436 and #463. Only after #463 merges and the exact source revision passes
native/Windows, Studio, Electron and required CI can its full 40-character
**merge SHA** be entered below. No accepted Windows SHA is asserted here.

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
# After #463 merges and the accepted exact SHA is formally recorded:
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
Desktop, and stages a fresh x64 PortAudio DLL from the same pinned
`147dd722548358763a8b649b3e4b41dfffbcfbb6` commit as CI.
Build evidence is local under `build_reports/windows-setup/summary.txt` and
`protocol-tests.log`; PortAudio and temporary profiles stay in
`.tools/windows-setup/`. Do not upload logs containing machine-specific paths.

Tests must actually execute, not be skipped. After the native suites pass,
the optional Electron probe launches with `--null-audio`, temporary native
`--settings`, and explicit isolated Electron `userData` and `sessionData`.
It checks for an actual sidecar hello and kills its owned process tree after
30 seconds. **This probe alone does not certify the visible window, UI-ready
state, patch round-trip, or clean user-initiated shutdown.** Those require a
real executed Windows acceptance report; until then W1 is blocked.

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
