<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Architecture today

This is the implemented native architecture after the M0 clean-copy migration.
The roadmap is in [PLAN.md](PLAN.md); evidence and limits are in [STATUS.md](STATUS.md).
The app is alpha. Experimental offline ABI2 proofs are distinct from the live ABI1 app.

## Product boundaries

Wirescape is a standalone Qt Quick application, with a C++17 headless engine.
Qt, Python build tools and device APIs stay outside DSP processing. No browser hosts the GUI.
The original Python/browser prototype lives in `legacy/` as reference only, is not built or maintained,
and has no bundled Surge binaries. It preserves earlier musical/design ideas, not the current runtime.

## Active application: ABI1

- `native/include/wirescape/module.h`: experimental C module boundary; immutable descriptors,
  mono float AUDIO/CONTROL ports, numeric parameter IDs, lifecycle/event/process callbacks.
- `native/include/wirescape/engine.h`, `native/src/engine.cpp`: dynamic module loading,
  prepared mono DAG, preallocated buffers, one SPSC event producer and one render consumer.
  Topology changes happen quiescently. Cycles and multiple signal sources per input are rejected.
- `native/ui/studio_controller.*`: authoritative patch document, validation, staged replacement,
  generic module catalogue, parameters, save/load and offline rendering.
- `native/document/`: optional QtCore legacy inspector and native v1/v2 reader, lossless v1
  migration, relational validation and atomic v2 writer with v1 backups. The engine remains Qt-free.
- `native/ui/audio_device.*`: dynamically loaded PortAudio bridge, explicit start/stop,
  format probing, callback fault counters and cleanup ownership. Audio never auto-starts.
- `native/ui/qml/`: canvas, module library/cards, generic controls, cables, keyboard and dialogs.
- `native/control/provider_bridge.*`: offline single-owner timestamp/coalescing proof;
  no real MIDI/network service.

Processing/event/reset are bounded, allocation-free and nonblocking. Prepare/create/destroy
require stopped producer/consumer. Shared LinearRamp uses 5 ms sample counts; initial/reset
targets snap and identical repeated targets do not prolong a transition.

ABI1 examines at most256 events and renders64 graph spans per call. Overload silences that
call, advances time, increments counters and invalidates the generation. Panic changes generation
immediately, with reset at the next active render boundary; ABI1 has no applied-generation ack.
Device processing faults stop playback and require explicit restart.

Current notes are mono last-on/retrigger with source/note-matched envelope release, no held-note
fallback or sustain manager. Engine output is raw; device/WAV boundary applies gain0.2 once and
clamps. PortAudio duplicates the mono output into stereo. This is separate from ABI2 stereo buses.

## Current Essentials:13 ABI1 modules

VCO (sine), ADSR, VCA, Mixer, Output, Scale/Offset, LFO, Clock, Noise, Slew,
Function Envelope, VCF and Ladder Filter retain stable `essentials.*` identities.
VCA has no LPG mode; Mixer has two mono inputs. ADSR is event-gated, while Function Envelope
has a CONTROL gate and gated rise/fall/curve. Slew is linear, not exponential lag.
VCF exposes12/24dB taps of a four-pole TPT network; Ladder is bounded ladder-inspired DSP,
with documented approximation/aliasing limits. Neither is an exact analogue model claim.

Ordinary modules are folders under `native/modules/` with module.cpp/json/CMakeLists;
root CMake discovers them. The app discovers `ws_` libraries beside its executable.
Source copying alone does not install a module. Native code is trusted, not isolated.

## Isolated ABI2 foundation (review required)

`native/include/wirescape/module2.h`, `native/sdk/`, `native/engine2/`, `native/voice/`,
`native/modules2/` and `native/tests2/` contain a second, offline contract/engine proof.
It is not selected by the application and `ws2_` libraries do not replace ABI1 plugins.

The proof includes planar1–16 voice signals, CONSTANT views, normalized parameter modulation,
event outputs, deterministic SCC/back-edge delays, transport context, bounded producer queues,
fake-clock conversion, panic/applied-generation acknowledgement and multichannel output buses.
The fixed-storage allocator implements Rotate/Reuse/Reset/Mono, sustain and oldest stealing;
MPE is a stub. Five SDK ports (VCO/LFO/VCF/ADSR/VCA) and notes-to-CV/rows test modules prove
an offline eight-voice synth. Mixer, noise, output, scale/offset and slew also have SDK ports with exact default references, portable state and per-voice tests. The output port is a device-free signal node; the engine binds and mixes its voices into buses. A Ring Mod exercise tests the author guide.

Finite release capacity uses reserved slots then emergency global reset/choke; unbounded lossless
note-off delivery is not promised. PARAM_MOD currently targets explicit voice channels.
Latency reporting includes all scheduled forward dependencies, not only output-reaching audio.
Mono throughput misses the requested performance target. See STATUS before accepting this ABI.

SDK/header/voice are original MIT code. Engine/app and GPL-derived ports remain GPL.
Desktop C++17 separation is not a proof that the engine or every patch fits embedded hardware.
Thin adapters and actual ARM/CLAP/Daisy builds remain later experiments.

## Documents, examples and testing

Current documents use `wirescape.native.patch` v1. Unknown/incomplete module state/cables remain
editable and round-trip; failed preparation or unsupported input must not replace accepted data.
The document library supports v2 with finalized schema/example and a bit-identical demo render
through save/reload. App integration uses this library in M2-6.
`native/examples/Six_Essentials_Demo.json` demonstrates the six imported modules plus VCA/Output.
Synthetic legacy fixtures are public test inputs, not personal patches.

`tools/dev.py` orchestrates deterministic native build/test/report, UI checks, offline render,
benchmarks and local package checks. `tools/foundation/` supplies reporting/tests;
`tools/experiments/reference_kernels.py` is required by a helper test.
`tools/bench/abi2_foundation.py` collects ABI2 CTest/Debug CRT, hashes and WAV statistics.
Legacy-only tooling moved to `legacy/tools/`.

```powershell
$env:WIRESCAPE_QT_DIR = '<external-Qt-SDK-prefix>'
$env:CMAKE_BUILD_PARALLEL_LEVEL = '2'
python tools/dev.py doctor
python tools/dev.py build
python tools/dev.py test
python tools/dev.py check --package
python tools/dev.py launch
```

Qt discovery: explicit --qt-prefix, otherwise WIRESCAPE_QT_DIR, CMAKE_PREFIX_PATH,
then `.tools/qt/6.10.3/msvc2022_64`. No SDK or compiler is checked into this repo.
PortAudio is dynamically loaded via WIRESCAPE_PORTAUDIO or a library beside the executable.
Packaging accepts --portaudio, WIRESCAPE_PORTAUDIO or `.tools/portaudio/libportaudio64bit.dll`.
CI must acquire/build the backend separately and pin its dependency version.

The private optional font loads only with --local-font pointing to an untracked external file.
Normal startup uses system fonts and requires no proprietary font. Icon PNG/ICO assets are source
assets, not compiled application binaries. Shortcut installer takes a mandatory package path.
`build/`, `build_reports/`, `.tools/`, `user_data/` and private backups are ignored local-only data.
The generated build/cache/test reports contain local paths and must never be committed.
Manual real-device probes require explicit confirmation/authorization and are never in CTest.

## CI

`linux-arm64` runs the headless native/ABI2 suites and ABI2 benchmark on PR updates and pushes to `main`.
`windows-x64` runs MSVC/Qt 6.10.3 build, native tests and offscreen UI checks on main,
manual runs, ready PRs, or draft PRs labelled `ci/windows` (the label takes effect on the next push; label changes never start CI).
`guard` runs as a dedicated `pull_request_target` workflow and evaluates PR changes with the base branch's `tools/ci/guard.py`, reading the PR's labels live so re-runs see current labels.
PortAudio v19.7.0 is built at `147dd722548358763a8b649b3e4b41dfffbcfbb6`
under its MIT licence (`LICENSE.txt` upstream); `WIRESCAPE_PORTAUDIO` locates the DLL.
Summary JSON and ARM64 benchmark numbers are artifacts; full logs upload on failure.
`guard` enforces protected paths, test retention, private/binary exclusions and portable paths.
Protected CI changes require `allow-protected`; test removal requires `allow-test-removal`.

ABI2 non-loop components write directly to global output buffers and alias compatible signal inputs; reductions, delays and loop spans retain prepared scratch buffers.
The `sanitizers` job runs headless ASan+UBSan and TSan ABI2 suites on Linux ARM64, including bounded panic/egress stress, and appends callgrind Ir/block measurements to its summary.

## Audio capture harness

`tools/capture/` renders native patch v1 via ABI1 without opening devices and
compares signals using alignment, level, timing and response metrics. CTest's
`capture_offline_selftests` uses synthetic captures and a fully mocked runner.
`native/tools/capture/` builds a separate Windows SDK process-loopback helper;
`python tools/capture/run.py --confirm-audio` is the explicit manual entry point.
It captures only the dedicated helper's render stream on Windows build 20348+,
never microphones or an unisolated endpoint mix. Reports stay in `build_reports/`.
Device execution is unverified and reserved for the user; CI only compiles the
helper and runs offline tests. See `tools/capture/README.md` for bounds and scope.

Clock, function envelope and ladder have isolated ABI2 ports with per-voice state and exact ABI1 default references. Clock remains free-running; the existing filter stress functions also execute against ABI2 ladder through a test-only callback bridge.

Engine2 reports forward signal latency per output bus and feedback delay separately per SCC. Quiescent reconfiguration stages only the changed processor; untouched instances and compatible loop delay lines retain their state, with port-ID remapping committed after validation.

## J1 app integration candidate

`native/control/app_engine.*` adapts app control and metadata to engine2; built-in
Essentials use native ABI2 descriptors, while dynamically loaded ABI1 modules use
the shim. `native/document/app_patch.*` builds engine2 graphs from the v1/v2 reader.
The UI callback and CLI renderer call this facade. Integration is blocked pending
planner review of a nonzero Six Essentials reference-render delta.

The ABI2 built-in catalogue has one registration list,
`native/control/module_catalog.inc`: add `WS_APP_MODULE(Class)` and include the
module DSP header in `module_catalog.h`. Modules with external link dependencies
attach those dependencies to the `wirescape_catalog_dependencies` interface target
in their own CMake registration. The app engine links that target.

## J2 keyboard polyphony candidate

`native/voice/note_to_cv.h` (MIT) is the reusable event-to-voice adapter used by
`host.keyboard` and available to MIDI→CV. It preserves source identity, isolates
sustain by source/channel, forwards assigned-voice events, and emits choke before
replacement attack. Anonymous repeated-key releases are FIFO in the allocator.
Only the first Keyboard module receives app keys. New patches connect Keyboard
pitch to VCO and gate to ADSR. Output uses SUM input policy; voices collapse at
that module before the stereo device duplication. Cable badges display configured
signal widths propagated through the document graph without reading render-owned
metadata concurrently. Computer key-repeat suppression and focus-loss release
remain in the existing QML key handler. Panic resets every voice at render boundary.
