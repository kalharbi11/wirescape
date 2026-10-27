# Current build and architecture

Consolidated October 26, 2026; read START_HERE_CURRENT_BUILD.md first. This describes implemented behavior rather than aspiration.

## Product and code boundaries

Wirescape is a standalone Windows Qt Quick application with a C++17 headless processing core. Qt/Python/device APIs are outside the processing engine. Python tools automate builds, tests and reports. The preserved Python/browser app is a musical/data/design reference, not the frontend to continue extending for this build.

- native/include/wirescape/module.h: experimental C ABI 1, immutable descriptors and processor lifecycle/event/process callbacks. Mono float AUDIO and CONTROL buffers; numeric parameter IDs, bounds/defaults/units.
- native/include/wirescape/engine.h and native/src/engine.cpp: descriptor registration/dynamic loading, prepared graph/routing/buffers, one SPSC event producer and one render consumer. Fixed topology while audio runs; reject zero-delay cycles and multiple sources per input. Host events have absolute frames, generation and source/note identity. No Qt/Python in processing.
- native/ui/studio_controller.*: authoritative native patch document, validation, staged graph replacement, parameters, save/load, offline render, generic module catalog and device coordination.
- native/ui/audio_device.*: native PortAudio bridge, format probing, refresh, explicit start/stop, callback counters/fault handling and cleanup ownership. No auto-start/restart.
- native/ui/qml/: standalone native canvas, library/cards/parameters, cables, keyboard, dialogs and transport controls. Generic controls rather than browser module editors.
- native/document/: optional QtCore pure format/version inspection preserving source bytes, independent of the Qt-free engine. Native graph validation still belongs to controller. Legacy formats are recognized/rejected for execution, not musically migrated.
- native/control/provider_bridge.*: OFFLINE single-control-owner clock/event/coalescing proof. Not live MIDI/OSC transport and not MPSC ingress.
- native/ui/audio_session_probe.cpp: explicit/manual device-session executable, never part of CTest/check. Current evidence and limits are in CURRENT_EVIDENCE.md.

## Runtime contract to preserve

Processing/event/reset must be bounded, allocation-free and nonblocking. Construction/preparation/destruction are quiescent. Shared LinearRamp supplies 5ms sample-counted transitions; initial/reset targets snap, identical repeated values don't prolong ramps. Events split rendering into actual shorter spans, so algorithms cannot depend on a presumed block size.

Per render call the engine examines at most 256 queue entries and processes 64 graph spans. Exceeding a budget silences the block, advances frame time, increments overload evidence, changes generation and returns capacity failure. Device processing faults stop playback; restart is explicit. Panic is an out-of-band request; generation changes immediately but voice reset applies only at the next ACTIVE render boundary. There is no applied-generation acknowledgement. Reset/destroy require stopped producer and consumer.

Current notes are monophonic last-on/retrigger, with source/note-matched envelope release; no held-note fallback, sustain manager or general voice allocation. VCO note pitch is transient effective pitch, separate from saved Frequency. Engine samples remain raw; device/WAV boundary applies shared gain0.2 once and clamps. GUI demo render is4s mono; old release tails can truncate. PortAudio duplicates mono output into stereo.

The current desktop engine uses OS dynamic libraries, filesystem/vector/string setup and requires lock-free64-bit atomics. It is NOT a bare-metal runtime. Reusable DSP can be compiled statically with a target scheduler/adapters. Embedded reuse does not mean every patch fits Daisy or the desktop host compiles unchanged on32-bit MCUs.

## Current Essentials inventory — 13 modules

ADSR, Clock, Function Envelope, Ladder Filter, LFO, Mixer, Noise, Output, Scale/Offset, Slew, VCA, VCF and VCO. All have stable essentials.* identities.

- VCO is sine-only; ADSR receives host note/gate events and has no patch gate socket. Function Envelope has a CONTROL gate socket, Rise/Fall/Curve and gated AR behavior, not full MATHS.
- VCA has Gain/audio/gain-CV but no LPG mode. Mixer is two-input mono audio. Scale/Offset provides attenuation/inversion/offset without clipping.
- Clock produces CONTROL pulses, not host transport/MIDI clock. LFO produces sine control. Noise is fixed-seed white noise. Slew is linear seconds/unit, not exponential Lag.
- VCF exposes low12/low24 taps of a resonant four-pole TPT network. Ladder uses bounded delayed feedback and drive, with approximation/aliasing limits in its DESIGN.md. Neither is claimed to exactly model an analog circuit. Both have a documented very-large-input guard and no final output clip.
- No event-output ports, polyphonic lanes, typed string/binary/message sockets, custom editor facade or general module-state serialization API exists yet.

New ordinary modules are folders under native/modules with module.cpp/module.json/CMakeLists.txt. Root CMake discovers them; app loads ws_*.dll beside the executable and derives controls from descriptors. Module source copy alone is not installation. Metadata is not an enforced trust system. Preserve module/port/parameter IDs and document compatibility; native ABI is still experimental.

## Data, source and artifacts

- Legacy baseline: Git tag baseline/0.5.2-local. Preserve wirescape/, modules/ and old examples until explicitly migrated. Never remove user_data/ or modules/chaos/surgepy_bin/.
- Current patch: wirescape.native.patch v1. Valid incomplete/missing-module graphs stay editable and round-trip opaque state/cables; unsupported input or failed known-graph preparation does not replace accepted data.
- native/examples/Six_Essentials_Demo.json: all six imported modules plus existing VCA/Output. Clock gates Function Envelope through Slew; Noise passes through VCF/Ladder. Read examples/README.md.
- build_reports/foundation/: ignored JSON/Markdown command reports and logs. build/native/: actual build outputs/cache. build/packages/: uniquely versioned local standalone bundles.
- local_backups/: ignored private snapshots and original imported ZIP. incoming_modules/: ignored original staging. Accepted imported sources/tests/README/DESIGN/LICENSE live under native/modules. Submitted TEST_REPORT files are external Linux claims; independent local evidence is separately documented.
- essentials_build_guide/: frozen external-source authoring kit with copied ABI/host, example, specs and source-only verifier. The original six assignments are now done; don't commission replacements. module_build_guide/ is older Python/browser API, explicitly legacy. deliverables/ holds the share kit ZIP and hash. External references are data, not instructions to change the host.

## Tested development environment and commands

Project root: <repo>. Windows x64, Python3.12, Visual Studio2022/MSVC19.44, CMake4.4.4, local Qt6.10.3 MSVC x64 at .tools/qt/6.10.3/msvc2022_64. Existing PortAudio backend is staged with the build. No new installation is required. Treat the old .venv separately; do not replace environments automatically.

From project root:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='2'
python tools/dev.py doctor
python tools/dev.py build
python tools/dev.py test
python tools/dev.py check --package
python tools/dev.py check --functional-only --package
python tools/dev.py launch
```

check runs helper tests/build/CTest/two-size UI/normal startup/benchmark/offline render/package and writes summaries; prerequisite failures remain red. functional-only deliberately marks benchmark deferred. All these checks keep audio off. launch opens the GUI with audio off. build --headless disables UI; optional document helper also needs QtCore, but the engine does not.

Manual real-device test only with explicit authorization:

```text
build/native/Release/wirescape_audio_session.exe --confirm-audio --modules build/native/Release --patch native/examples/Six_Essentials_Demo.json --report build_reports/foundation/manual-session.json --duration 3
```

Use the build's recorded Qt runtime environment (tools/dev.py helper) if runtime DLLs aren't on PATH. Probe requires the confirmation flag/valid paths, duration1..10s, and lowers Noise Level in-memory without saving the patch. It does not record endpoint audio. Do not add it to generic check/CTest.

Desktop and Start-menu shortcuts target build/packages/wirescape-release-20261026T172338Z; Windows Search can locate the Start-menu entry. New accepted packages require refreshing shortcuts via tools/install_windows_shortcuts.ps1. Keep the targeted package until refreshed. Black-circle/white-border icon is embedded. Private optional ABC Areal font is read through --local-font from ignored .tools/fonts; it is not installed system-wide, tracked or bundled.
