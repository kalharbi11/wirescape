<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape native engine

The native product is the C++17 realtime engine and its `wirescape-engine`
sidecar. Windows source bootstrap and device-off checks are documented in the
[install guide](../docs/INSTALL.md).
The Electron application in `apps/desktop` hosts Studio from
`apps/studio`; use the [repository quickstart](../README.md) to build all three.

From the repository root:

```sh
python tools/dev.py build --headless --config Release
python tools/dev.py test --config Release
```

Use `python3` on the Pi. `--headless` builds the native engine, CLI and
physical-device-free tests without Qt. The supported app is Electron + Studio
with the Qt-free `wirescape-engine` sidecar. The former Qt UI and QtCore document
inspector were removed by [R3b](../docs/planner/CLEANUP.md); their sources
remain in the immutable `archive/qt-final` Git tree, not the active build.
ABI1 engine and module sources remain only until the separate R6 retirement.

The sidecar uses bounded newline-delimited JSON over stdin/stdout, with logs on
stderr. It owns device services; Studio owns the editable patch document.
[ENGINE_PROTOCOL](../docs/specs/ENGINE_PROTOCOL.md) specifies the boundary.
ABI 2 module sources are in `modules2` and SDK examples in `sdk/examples`;
[the author guide](../docs/SDK_AUTHOR_GUIDE.md) describes stable IDs and tests.
Create/prepare/destroy run off the audio thread; rendering does no allocation,
locks, I/O or logging. Agents and CTest keep physical devices off.

For an offline CLI check, without opening PortAudio:

```sh
build/native/wirescape_cli play native/examples/Six_Essentials_Demo.json --seconds 1 --output build/example.wav
```

Windows uses the `Release/wirescape_cli.exe` executable under the same build
folder. Physical playback requires the owner and explicit device-confirmation
flags. See [current evidence and pending gates](../docs/STATUS.md); successful
compilation or an installed PortAudio library alone does not verify sound.

The project retains its root GPL license. The shared vendored PortAudio header
and its upstream license are now under `native/third_party/portaudio/`; see
[PortAudio license](third_party/portaudio/PORTAUDIO_LICENSE.txt) and
[audio backend provenance](io/audio/DESIGN.md).
