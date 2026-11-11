<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Install Wirescape 1.0 from source

Wirescape 1.0 is the Electron app (Studio patch editor plus the native
`wirescape-engine`). There is no installer yet; you build one exact commit from
source. The old Qt app is retired, so don't use its shortcuts, packages or
`--qt-prefix` launches. What's in 1.0 and how to report a test:
[releases/v1.0.md](releases/v1.0.md).

- [Windows 10/11 x64](#windows-1011-x64)
- [Raspberry Pi 5](#raspberry-pi-5)
- [Update to a later version](#update-to-a-later-version)
- [Troubleshooting](#troubleshooting)

## Windows 10/11 x64

### 1. Install once

| Tool | Version | Notes |
|---|---|---|
| Git | any recent | |
| Visual Studio 2022 or 2026 | C++ x64 workload + Windows SDK | Build Tools edition is enough |
| Python | **3.12 x64** with the `py` launcher | `py -3.12 --version` |
| Node.js | **22 LTS x64** (npm included) | `node --version` must print `v22.*` |
| CMake + Ninja | CMake 4.2.3, Ninja 1.13.0 | `py -3.12 -m pip install cmake==4.2.3 ninja==1.13.0` |

Open **"x64 Native Tools / Developer PowerShell for VS"**, not a plain
PowerShell. The setup script checks every tool and stops with a message if one
is missing.

### 2. Get a clean checkout of the version

Use a **new folder** for each version. Your old folder, patches and settings
stay untouched, and stale Qt build caches can't leak in.

```powershell
git clone https://github.com/kalharbi11/wirescape.git C:\src\wirescape-1.0
cd C:\src\wirescape-1.0
# 1.0 is the commit named in docs/releases/v1.0.md. Before it is pinned, use current main:
$Ref = (git rev-parse origin/main).Trim()
git switch --detach $Ref
```

### 3. Build and self-test (devices stay off)

```powershell
.\tools\windows\setup.ps1 -CheckOnly        # read-only preflight: prints "Preflight PASS"
.\tools\windows\setup.ps1 -Ref $Ref         # about 10-20 min the first time
```

The script:
- builds pinned PortAudio and puts `libportaudio64bit.dll` next to the engine;
- builds the engine (`build\native\Release\wirescape-engine.exe`);
- runs the engine protocol tests;
- builds Studio and the desktop app;
- opens the real app once with audio off, as a smoke test.

The last line it prints is
`status=windows-device-off-smoke-passed` in `build_reports\windows-setup\summary.txt`.
If you see `W1 failed: …`, the message and `build_reports\windows-setup\`
say which step failed.

### 4. Launch with sound and MIDI

```powershell
.\tools\windows\Wirescape.cmd
```

This is the owner launcher. It enables audio and MIDI (`--confirm-audio --confirm-midi`). Close
any DAW that holds the audio device exclusively. With shared WASAPI, set Wirescape to
the device's own sample rate; 48 kHz with a buffer of 256 is the known-good setting.

**ASIO:** setup builds PortAudio with ASIO, so installed ASIO drivers (your
interface's driver, FlexASIO, ASIO4ALL) appear in the audio device list as
`ASIO` entries. Pick one there; buffer size and sample rate for ASIO are often
set in the driver's own control panel (FlexASIO: `%USERPROFILE%\FlexASIO.toml`).
ASIO opens one driver at a time, so close Ableton or any other ASIO app first.
If the ASIO SDK download fails, rerun setup with `-NoAsio`.
Settings live in `user_data\settings.json` in this checkout. Patch files you saved
earlier stay where you saved them: open them with **Load**.

Device-off launch (silent, for checking the window only):

```powershell
& .\apps\desktop\node_modules\.bin\electron.cmd .\apps\desktop\build\main.js --engine "$PWD\build\native\Release\wirescape-engine.exe" --null-audio
```

## Raspberry Pi 5

Use 64-bit Raspberry Pi OS (desktop). The full owner runbook, with package list,
preflight, isolated profile and rollback, is [PI_FRESH_INSTALL.md](PI_FRESH_INSTALL.md).
In short:

```sh
python3 tools/pi/preflight.py --check-only
sh tools/pi/update-and-build.sh <full commit SHA>
python3 tools/dev.py test
python3 tools/pi/app_smoke.py --report build_reports/pi-app-smoke
apps/desktop/node_modules/.bin/electron apps/desktop/build/main.js \
  --engine "$PWD/build/native/wirescape-engine" --null-audio
```

The Pi needs Node 22, Python 3.12, `build-essential`, `pkg-config`,
`libasound2-dev` and `libportaudio2`. Only build a commit that has Pi evidence
recorded in the release notes. Until 1.0 gets its own Pi run, the last
Pi-certified commit is `cd7999e`.

## Update to a later version

Versions are 1.0, 1.1, 1.2, … ([releases/README.md](releases/README.md)).
For each new version, repeat Windows steps 2–4 in a **new folder**, using
that version's commit from its release notes. Keep the previous folder until
the new one passes your test; switching back just means running the old
folder's `tools\windows\Wirescape.cmd`.

## Troubleshooting

| Message | Fix |
|---|---|
| `Use Windows x64 Developer PowerShell…` / `cl.exe missing` | Open the VS x64 Developer PowerShell, not plain PowerShell. |
| `Node.js 22 LTS x64 required.` | Install Node 22 LTS x64. Node 24/26 are not supported on Windows yet. |
| `Python 3.12 x64 with py launcher required.` | Install Python 3.12 x64 and tick "py launcher". |
| `Stale/Qt-enabled CMake cache.` | You reused an old folder. Do step 2 again in a new folder. |
| `Source worktree is dirty` | You changed files in the checkout. Use a fresh folder. |
| No sound | Check the output device and rate in Settings, close any DAW that holds the device, try 48 kHz / 256. |
| Crackles | Try an `ASIO` device (FlexASIO or your interface driver), then a 512 buffer; copy any `overrun` lines from the terminal into your report. |
