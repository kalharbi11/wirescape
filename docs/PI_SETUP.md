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


## Wirescape standalone app: separate source preparation gates

The standalone app is Electron + Studio + the native `wirescape-engine`. Its
source build is **not** a desktop install or a release. The owner starts the
installation step only after the separate Windows test decision. Agents and
the Pi runner do not install packages, alter sessions, open devices, or SSH.

### 1. Check prerequisites (read-only and offline)

On the Pi 5 with 64-bit Raspberry Pi OS, from the intended checkout:

```sh
python3 tools/pi/preflight.py --check-only
```

The command never fetches, checks out, builds, installs, launches Electron,
or writes a report. It verifies Linux ARM64, compiler, Git, Python, CMake,
Ninja, Node/npm, tracked npm lockfiles, libportaudio.so.2 and Electron
shared-library prerequisites, disk/permissions, Git cleanliness, inherited
Qt overrides, and stale cross-checkout/architecture CMake caches.
Supported maintained Node LTS majors are **22, 24, and 26**. Legacy Qt
runner tests have separate requirements; Qt is not required to build the
source Electron app. A failure names an owner-managed action. Do not solve
a failure by deleting the owner's cache or rewriting global environment.
Use the distro's package manager manually for missing native libraries.

### 2. Update and build the exact approved source

From a clean checkout, with owner-approved immutable SHA:

```sh
sh tools/pi/update-and-build.sh <accepted-commit-sha>
# optional bounded native concurrency override:
sh tools/pi/update-and-build.sh <accepted-commit-sha> --jobs 4
```

The historical one-argument ref invocation remains supported. It preflights
before fetch, resolves the supplied ref, verifies the resulting exact SHA
after detached checkout, preflights the selected source again, then calls
the existing `tools/dev.py build --headless --config Release` driver
(default **2** native jobs; explicit **1–4**). Studio and Desktop use their
tracked lockfiles with `npm ci`. Native, Studio install/build, and
Desktop install/build stages fail fast without running later stages.

Generated, ignored diagnostics are local to
`build_reports/pi-source-build/<sha>/`: stage logs (sanitized,
size-bounded), per-stage JSON, and `summary.json` (resolved SHA,
tool versions, architecture and stage outcomes). On failure, inspect the
named stage log, repair the cause manually, then rerun the same SHA.
Reports are not commits and are not evidence of real Pi execution by
themselves. The first native configure may download pinned sources;
`npm ci` requires registry access. The check-only command does not.

### 3. Test and inspect evidence (devices off)

```sh
python3 tools/foundation/tests/test_pi_preflight.py
python3 -m unittest discover -s tools/foundation/tests -p test_pi_update.py
python3 -m unittest discover -s tools/foundation/tests -p test_build_parallel.py
python3 tools/dev.py test
```

The private Pi runner must execute its task-specific `pi-run:` acceptance
with `ci/pi-accept`; a hosted or mock x86 pass is not a Pi result.
A missing Pi dependency is a BLOCKED setup step, not a skipped success.

### 4. Owner-only installation and device launch (deferred)

After the owner's Windows test and explicit go-ahead, the separate
`tools/pi/install-desktop-entry.sh` can create a per-user desktop
entry. It is **not** called by check-only or the source build. Only the owner
invokes the launcher with `--confirm-audio --confirm-midi`. For device-free
diagnostics, use a temporary native `--settings` path and `--null-audio`
without either confirmation flag; see the app smoke task (#446).
Device choices belong in the owner's `user_data/settings.json` unless
explicitly redirected. The older `~/.cache/wirescape/demo.log` is for
the retired class-demo kiosk, not Electron. See #247 and #253 if the
native sidecar or desktop entry is absent.

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
