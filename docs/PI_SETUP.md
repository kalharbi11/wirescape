<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Raspberry Pi 5: headless playback

Use a 64-bit Linux distribution and a USB audio interface with ALSA support.
The user installs build tools and PortAudio (agents never run sudo):

```sh
sudo apt install build-essential cmake ninja-build python3 libportaudio2 libasound2-plugins
python3 tools/dev.py build --headless
python3 tools/dev.py test
```

The engine, Qt-free document loader and CLI build without Qt. FetchContent downloads
nlohmann/json at its pinned commit; Internet access is required on first configure.
QtCore document editing tests are optional. The Pi UI remains a stretch; the
headless runner is the supported checkpoint path.

First verify an offline patch. Without `--confirm-audio`, `play` renders ten seconds
to `build/play-offline.wav` and prints peak/RMS and block timing statistics:

```sh
build/native/wirescape_cli play native/examples/Six_Essentials_Demo.json
```

Only the user runs these real-device commands:

```sh
build/native/wirescape_cli play --list-devices
build/native/wirescape_cli play native/examples/Six_Essentials_Demo.json --confirm-audio --device 2 --rate 48000 --block 128
```

Replace `2` with the USB interface's index or exact saved device name. PortAudio
loads `libportaudio.so.2`; ALSA plugins may route to PipeWire/PulseAudio. The runner
uses stereo float output, applies gain/clamp once at the device boundary and prints
`cpu=<percent> xruns=<count> faults=<count>` once per second. Increasing xruns means
the device missed output deadlines; try block 256 and close competing applications.
Ctrl+C stops services, then render, then destroys the engine. MIDI and OSC option
names are reserved for J4/J5; they currently fail with a clear message.

For a device-free ten-minute loader soak (115 MB stereo WAV, no audio backend):

```sh
build/native/wirescape_cli play native/examples/Six_Essentials_Demo.json --seconds 600 --pace --rate 48000 --block 128 --output build/loader-soak.wav
```

Troubleshooting:

- **Device busy:** close other audio applications or select its PipeWire/Pulse ALSA route.
- **Wrong rate:** start at 48000 Hz; inspect the interface's supported rates and choose explicitly.
- **Permissions:** check `/dev/snd` permissions and the distribution's session/audio group policy; log in again after user-managed group changes.
- **Library missing:** install `libportaudio2`; `libasound2-plugins` supplies common server routes.
- **Silent patch:** inspect the offline peak/RMS, output bus and gate wiring before opening a device.
- **Patch refused:** only native v1/v2 patches load. Unknown modules/state/config are reported; originals are never rewritten by `play`.


## Wirescape standalone app on the Pi (Phase 1, owner only)

The standalone application is **Electron + Studio + wirescape-engine**; the
earlier class-demo kiosk below is a separate, deferred Phase 0 workflow.
Use Raspberry Pi OS 64-bit with a graphical desktop session. Install a current
Node.js LTS and npm, C++ build tools, CMake, Ninja, Python 3, and the shared
PortAudio runtime (\`libportaudio2\`). The first native configure may fetch pinned
sources, and \`npm ci\` needs registry access. No device is opened by a build.

From a **clean** checkout, use an approved immutable SHA or \`origin/main\`:

\`\`\`sh
sh tools/pi/update-and-build.sh <accepted-commit-sha>
sh tools/pi/install-desktop-entry.sh
\`\`\`

The update script fetches, detaches at the resolved commit, builds the Release
engine (\`build/native/wirescape-engine\`), installs the pinned Studio and
desktop npm dependencies and builds both. It stops on error and prints the
owner's launch command. Subsequent builds reuse the native build directory.
\`install-desktop-entry.sh\` is part of P1F (#253); it creates a **per-user**
desktop entry and launcher, never a service or system-wide install. The
launcher passes \`--confirm-audio --confirm-midi\` only when the owner starts
the app. For device-free troubleshooting, launch from a terminal with
\`--null-audio\` and **without** either \`--confirm-*\` flag.

The engine's persisted device choices belong in \`user_data/settings.json\`
(or the explicit \`--settings\` file), **not** in the patch document. App
diagnostics reach the launching terminal or desktop session's journal; this
update script does not create an application log. \`~/.cache/wirescape/demo.log\`
belongs to the older \`demo.sh\` kiosk, not the Electron app. Saved patches
remain wherever the native file dialog saved them.

When the desktop entry or engine is missing, confirm that #247 (sidecar) and
#253 (desktop shell/launcher) are merged into the selected SHA and that
\`apps/studio/dist/index.html\` and \`apps/desktop/build/main.js\` exist.
Only the owner performs the physical audio/MIDI checks in \`DEMO_CLASS.md\` §6.

## Class demo (owner only)

Follow [the class rehearsal runbook](DEMO_CLASS.md). Build the accepted native
revision and UI (`npm ci && npm run build` in the UI checkout), then copy
`tools/pi/demo.env.example` to `~/.config/wirescape/demo.env` and edit its quoted
paths and exact endpoint names. `play --list-midi --confirm-midi` lists input
`name|system_name` pairs without opening ports; only the owner enumerates devices.
`tools/pi/demo.sh --dry-run 1` prints the patch and argv without launching anything.
`tools/pi/demo.sh 1` starts sound, MIDI and the local UI kiosk; `2` / `3` switch
patches with a short stop/start gap. `status` shows owned PIDs/log; `stop` stops
only the launcher's recorded processes. The web UI uses FakeEngine in Phase 0;
it does not control the audible native patch yet. The env file is trusted shell
configuration: keep it private. No autostart or service is installed.
