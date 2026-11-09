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


## Standalone Electron app: source preparation (not installation)

Electron + Studio + `wirescape-engine` source build is **not** an installer.
Installation follows the owner's Windows test; agents/Pi CI never install,
alter sessions, SSH, or open devices.

### 1. Check — offline and read-only

`python3 tools/pi/preflight.py --check-only` checks ARM64, tools, npm
lockfiles, PortAudio/Electron libraries, space, permissions, Git cleanliness,
Qt overrides and foreign/Qt-enabled caches. Node LTS **22/24/26** is required;
the Pi runner's Node 20.19.2 needs **owner-managed Node 22** (nvm/NodeSource)
to match CI. Qt is not required. No fetch, build, network, report, or
cache deletion; resolve failures manually.

### 2. Build approved source

```sh
sh tools/pi/update-and-build.sh <accepted-commit-sha>
sh tools/pi/update-and-build.sh <accepted-commit-sha> --jobs 4  # optional 1–4
```

The positional ref form remains. It checks cleanliness, preflights before
fetch and after detached SHA verification, runs the existing headless
`tools/dev.py` driver (default **2** jobs), then locked Studio/Desktop
`npm ci` builds. Stages fail fast. Only build/npm stages may use network.
Ignored `build_reports/pi-source-build/<sha>/` has sanitized logs and JSON
SHA/tool/stage provenance; inspect failed logs, then retry the same SHA.

### 3. Test — devices off

```sh
python3 tools/foundation/tests/test_pi_preflight.py
python3 -m unittest discover -s tools/foundation/tests -p test_pi_update.py
python3 -m unittest discover -s tools/foundation/tests -p test_build_parallel.py
python3 tools/dev.py test
```

Only exact-head private `pi5/accept` satisfies `pi-run:`; cloud mocks and
missing prerequisites do not count as Pi success.

### 4. Owner-only installation and launch — deferred

After Windows testing and approval, only the owner may run
`tools/pi/install-desktop-entry.sh` and launch with
`--confirm-audio --confirm-midi`. For device-free smoke use temporary
`--settings` and `--null-audio` (#446); ordinary device choices remain in
`user_data/settings.json`. No check/build stage installs or opens devices.

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

## Realtime audio priority (owner)

The desktop launcher passes the device-confirmation flags, but the owner must grant realtime scheduling permission separately. On distributions that use the audio group with PAM limits, add the user to that group, then create /etc/security/limits.d/99-wirescape-audio.conf with:

    @audio - rtprio 90
    @audio - memlock unlimited

For example, an owner can run `sudo usermod -aG audio "$USER"`, create the limits file with the two lines above, and log out and back in so the group and PAM limits apply. These are manual owner setup steps; Wirescape scripts never edit system files. Follow the distribution policy if it manages audio permissions through another group or session service.

To verify scheduling policy and priority, find the running engine PID and inspect it with `chrt -p`:

    pid=$(pgrep -fo '(^|/)wirescape-engine( |$)')
    chrt -p "$pid"
    grep '^VmLck:' "/proc/$pid/status"

A realtime policy such as SCHED_FIFO with a nonzero priority confirms realtime scheduling. VmLck reports locked memory; a zero value can be normal when the engine has not locked memory pages.
