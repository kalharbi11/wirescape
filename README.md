<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape™

An open-source modular patching environment for music, in alpha. The application
is an Electron window around Studio, the TypeScript patch editor, connected to
the native C++17 `wirescape-engine` process. ABI 2 modules provide polyphonic
cables, audio-rate modulation, feedback and portable DSP.

**Downloadable installers are not available yet.** Build from source at a
revision accepted for your test session. Current evidence: [STATUS](docs/STATUS.md).

## Build from source

Use Git, Python 3.12+, Node.js 22 with npm, CMake 4.2.3, Ninja 1.13.0 and a C++17
compiler. Windows x64 needs the Visual Studio 2022 C++ workload and Windows SDK;
Raspberry Pi 5 needs a 64-bit Linux desktop and C++ build tools. The engine build
below explicitly disables the retired UI. First configure and `npm ci` download
pinned dependencies; builds do not open audio or MIDI devices.

From the repository root, build the engine and the Electron application:

```sh
python tools/dev.py build --headless --config Release
python tools/dev.py test --config Release
npm --prefix apps/studio ci
npm --prefix apps/desktop ci
npm --prefix apps/desktop run build
```

On the Pi, use `python3` in place of `python`. For a clean checkout at an accepted
revision, [the Pi setup notes](docs/PI_SETUP.md) also document the owner-run
updater. The new isolated Pi preflight/app guide is being validated in
[#450](https://github.com/kalharbi11/wirescape/pull/450),
[#452](https://github.com/kalharbi11/wirescape/pull/452) and
[#453](https://github.com/kalharbi11/wirescape/pull/453); a common accepted SHA
for that path is still pending.

Open a terminal in your desktop session and try a device-off launch first.
Windows PowerShell:

```powershell
& .\apps\desktop\node_modules\.bin\electron.cmd .\apps\desktop --engine "$PWD\build\native\Release\wirescape-engine.exe" --null-audio
```

Raspberry Pi desktop terminal:

```sh
apps/desktop/node_modules/.bin/electron apps/desktop --engine "$PWD/build/native/wirescape-engine" --null-audio
```

Real playback needs PortAudio: the matching x64 DLL beside the Windows engine,
or `libportaudio.so.2` on Linux. Only the owner launches with `--confirm-audio`
and `--confirm-midi`. Follow the merged [Windows source setup guide](docs/WINDOWS_SETUP.md)
for the isolated bootstrap and pinned PortAudio path. A combined accepted source
SHA is still pending; preserve existing patches/settings. The owner's
first Windows session worked with WASAPI headphones at 48 kHz; another DAW must
release the device, and shared WASAPI must use its device rate.

## Development and project map

[CONTRIBUTING](CONTRIBUTING.md) · [agent rules](AGENTS.md) ·
[plan](docs/PLAN.md) · [architecture](docs/ARCHITECTURE.md) ·
[SDK guide](docs/SDK_AUTHOR_GUIDE.md) · [ABI 2 contract](docs/specs/ABI2_PLAN.md).
Studio lives in `apps/studio`, Electron in `apps/desktop`, native code in `native`,
and build/report tools in `tools`. Browser development uses `npm run dev` in
Studio; automated tests keep devices off.

The app and first-party modules are GPL-3.0; the SDK, ABI 2 header and voice
library are MIT. See `LICENSE` and `LICENSES/MIT.txt`. Third-party licences remain
with their components. Wirescape and its logo are [trademarks](TRADEMARKS.md).
