<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Architecture today

This is the implemented native architecture after the M0 clean-copy migration.
The roadmap is in [PLAN.md](PLAN.md); evidence and limits are in [STATUS.md](STATUS.md).
The app is alpha. Experimental offline ABI2 proofs are distinct from the live ABI1 app.

## Product boundaries

The current desktop application uses the Electron shell, Studio renderer and
Qt-free C++17 `wirescape-engine` sidecar. The earlier Qt Quick/ABI1 application
remains in-tree during staged retirement, but is not the Electron renderer.
Python build tools and device APIs stay outside DSP processing.
The original Python/browser prototype lives in `legacy/` as reference only, is not built or maintained,
and has no bundled Surge binaries. It preserves earlier musical/design ideas, not the current runtime.

## Web UI

`apps/studio/` contains the TypeScript/Vite UI imported from Wirescape-UI at
`da600d852d09415aa39e6d48106e3e58f0dd8aa9`, with complete Git history
(see [import record](../apps/studio/IMPORT.md)). Its patch document, catalogue,
canvas, panels and shared animation ticker currently run against `FakeEngine`.
The existing Playwright tests cover the UI without opening audio/MIDI devices.
The P1F Electron shell in `apps/desktop/` owns the native window, local-only
`app://wirescape` renderer resources, native file dialogs, and supervised
`wirescape-engine` process. Its renderer interface is the isolated five-member
`window.wirescape` preload object; commands and notifications use bounded NDJSON
over the engine child's stdin/stdout with no socket or HTTP server. The built UI
selects P1C's `RealEngine` when the native bridge is present and retains
`FakeEngine` for browser tests. Native File-menu intents use that bridge without
exposing file paths to the renderer.
A protocol stub with null audio/fake MIDI is used by desktop Playwright tests.
The Qt UI remains buildable as a migration baseline.

### Merged feature integrations (2026-11-10)

These additions are merged, but each device or runtime claim has its own gate:

- [Audio route metadata](https://github.com/kalharbi11/wirescape/pull/399) reports
  backend/output identity and compatible rates. The settings UI refuses a
  missing, ambiguous, mono or unsupported route rather than silently replacing it.
- [MIDI Learn](https://github.com/kalharbi11/wirescape/pull/500) maps a modulatable
  knob to a `midi.cc_to_cv` route through Studio with one undoable document edit;
  Forget removes only that mapping. Physical-controller tests remain separate.
- [OSC slice A](https://github.com/kalharbi11/wirescape/pull/518) registers the
  bounded `osc.in` and `monitor` ABI2 modules and their codec/tests. A current
  sidecar UDP worker, consent handling and Monitor presentation are not yet
  delivered by this slice; they remain separate work under #72.
- [CHAOS UI presets](https://github.com/kalharbi11/wirescape/pull/522)
  add eight unsaved preset labels and one-step Undo; the additive
  [LFO shapes](https://github.com/kalharbi11/wirescape/pull/533) preserve old
  parameter IDs and the sine default; the
  [CHAOS Lab patches](https://github.com/kalharbi11/wirescape/pull/531)
  add three release-safe library examples.
- The Groove feature inventory now includes
  [measured playhead/loop telemetry](https://github.com/kalharbi11/wirescape/pull/501),
  [native track lengths/gates/conditions](https://github.com/kalharbi11/wirescape/pull/505)
  and the [Studio editor](https://github.com/kalharbi11/wirescape/pull/509)
  with [roll/bar/scale tools](https://github.com/kalharbi11/wirescape/pull/516).
  These are merged capabilities, not evidence of later chord/speed work.


The root `web` workflow uses hosted Ubuntu and pinned Node 22 for `npm ci`,
typecheck, build and Playwright with the runner's Chromium. Failures retain
screenshots and traces as artifacts. The old UI-repository workflows live under
`apps/studio/tools/ci-reference/` as inactive reference, including its Pi job;
no public-repository workflow targets the Pi.

The Qt-free MIDI service uses its existing acquisition worker to detect endpoint
loss/return. Saved system identities keep same-name ports separate; source-only
chokes precede reconnect callbacks. Sidecar device pushes reflect worker snapshots
while enabled selections remain remembered. Fake hot-plug tests cover these paths
without opening hardware; no audio-thread polling or new protocol command is used.

## Legacy Qt application: ABI1 (retained during retirement)

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

## ABI2 foundation (SDK still experimental)

`native/include/wirescape/module2.h`, `native/sdk/`, `native/engine2/`, `native/voice/`,
`native/modules2/` and `native/tests2/` contain the ABI2 engine, module contract
and offline proofs. The Electron sidecar uses ABI2 through `AppEngine`; its SDK
and port equivalence tests are not substitutes for live-device verification.
The Qt/ABI1 plugin host remains a separate legacy build during retirement.

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

Live topology edits stage a complete candidate before a bounded render-park
handshake. Commit swaps prepared graph layouts into the existing Impl, retaining
surviving Nodes/DSP, producer rings, queued events and unchanged feedback delays.
Render fades out/in within one block and advances its clock while parked silent;
it never waits on control. Ack versus timeout has one atomic winner. Target
validation runs on render drain, with packet revisions rejecting removed/reused
node targets. Removed event edges receive a resume choke. Replan discards
counted stale future events routed through removed edges while retaining
external future events for surviving targets. The control facade
retains its catalogue and rebinds staged ABI1 descriptors before retirement.
Device and input-selection controls remain stop-only.

## J1 app integration candidate

`native/control/app_engine.*` adapts app control and metadata to engine2; built-in
Essentials use native ABI2 descriptors, while dynamically loaded ABI1 modules use
the shim. `native/document/app_patch.*` builds engine2 graphs from the v1/v2 reader.
The UI callback and CLI renderer call this facade. The planner accepted the app-only
Six Essentials reference-render delta (<=1e-7); explicit Noise default initialization
differs between ABI1 double-ramp and ABI2 cached-float parameter handling. Per-module
equivalence remains exact. Qt-backed app-document and real-loader soak evidence is
still required before this integration is ready.

With Qt Core available at build time, `wirescape_soak --patch <native-v1-or-v2.json>`
uses the app document reader, catalogue and callback facade, rather than a hardcoded
graph. The Pi evidence command is `wirescape_soak --patch
native/examples/Six_Essentials_Demo.json --rate 48000 --block 128 --seconds 600`.
Without Qt development files, the command fails explicitly instead of substituting
a graph. The six browser-era legacy fixtures remain non-executable, byte-preserved
documents; they are never silently converted to native patches.

The ABI2 built-in catalogue has one registration list,
`native/control/module_catalog.inc`: add `WS_APP_MODULE(Class)` and include the
module DSP header in `module_catalog.h`. Step Sequencer and Random Source share the
MIT SDK-example DSP headers and keep their stable `example.step_sequencer` and
`example.random_source` identities. Modules with external link dependencies
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

## J3 runtime candidate

`native/document/runtime_patch.*` is the Qt-free v1/v2 parser and graph builder
used by `app_patch` and `wirescape_cli play`. The pinned MIT nlohmann/json library
is used only quiescently. Qt document migration/preservation and the atomic writer
remain separate. Runtime input rejects duplicates, oversized/deep documents,
unknown modules/parameters and unsupported nonempty module config. Service-specific
config adapters are left to J4/J5 rather than silently dropping configuration.

Without `--confirm-audio`, play writes a ten-second stereo WAV under build and
reports peak/RMS and block timings. `--seconds 600 --pace` runs the loader soak.
Only explicit user confirmation reaches `native/io/audio/portaudio_service.*`,
which dynamically loads PortAudio and keeps backend code resident. Its callback
renders engine2 into preallocated stereo buffers; control code prints CPU/xruns.
Ctrl+C stops the loop and closes/terminates the backend before destroying the
engine. The Qt-free runner exposes reserved MIDI/OSC options that currently refuse
execution pending their services. See `docs/PI_SETUP.md` for the user commands.

MIDI ingress uses `native/io/midi/` (pinned libremidi plus a fake backend).
`native/control/midi_host.*` owns endpoint selections and the service lifetime;
producer 1 is reserved before engine preparation for timestamped D13 ingress.
Interactive Studio accepts an explicit, remembered Enable MIDI preference;
`--confirm-midi` remains supported for manual runner use. Offline, startup,
smoke, probe and CI-marked (`CI`/`GITHUB_ACTIONS`) paths ignore the interactive
preference and never create a real backend from it. Enabled endpoint names and system-name fallbacks are saved in
the patch's device records. The service exposes independent atomic receive
counts per selected endpoint, including ports sharing a display name.
Enabled input notes also reach existing `host.keyboard` nodes through producer1;
MIDI1/2 notes become source/channel-preserving note events. Existing `midi.in`
routes still receive raw events for explicit controller/converter routing.
Shutdown joins
MIDI producers and drains source chokes before stopping render or destroying its
engine. Raw MIDI releases use the same reserved queue classification as note
releases; MIDI 2 zero-velocity Note On remains a Note On.

The MIDI Host's internal live-replan seam stages subscriptions before committing
a candidate. A control-thread try-lock excludes only MIDI worker dispatch during
replan; backend callbacks keep their rings and audio never takes this lock. A busy
worker or refused candidate preserves both graph and routes. Keyboard conversion
is carried in the route table so new/removed sources cannot race a separate list.
The sidecar now calls this seam for incremental edits while audio renders.
A busy worker is retried three times at approximately 2 ms intervals, then
refused without changing the accepted graph. Non-busy replan refusal and
`loadGraph` use D5 in place: mute the live graph, hand the prepared candidate to
the running render thread through a lock-free `audio::EngineSlot` (adopted at a
block boundary by the PortAudio callback and the null sink alike), fade in, and
destroy the retired graph on the control thread. The device is never reopened
for a graph change, so Windows stream setup cannot break the 150 ms gap budget.
Stopped, not-yet-rendering and device-loss streams keep the stop/swap/start
path. Audio outputs are identified as `hostApi|name[#n]`. The null sink publishes render
liveness to control. On Windows its thread registers MMCSS "Pro Audio", falling
back to `THREAD_PRIORITY_TIME_CRITICAL`, and holds a successful 1 ms timer request
until stop. These scopes are released after the final render without device access.
`--bench-summary` reports fixed-size overrun diagnostics after the sink joins:
render duration and positive wake lateness maxima in microseconds, with counts
for renders longer than one period and starts after their scheduled time, among
overrun blocks only. Counts can overlap; neither changes the absolute completion
deadline test (`after > next`), including catch-up blocks.
Only accepted swaps advance the protocol generation.

## Stdio engine sidecar (P1B)

`native/sidecar` builds the Qt-free `wirescape-engine` process. Its reader and
writer isolate bounded NDJSON transport from the single control executor; audio
uses the existing AppEngine/PortAudio callback and MIDI host. Null mode runs a
paced discard sink and fake endpoints without device access. The UI owns saved
documents; sidecar candidates are disposable execution copies. Details and
shutdown ownership are in `native/sidecar/DESIGN.md`.

Audio loss leaves the same graph and remembered output identity owned by the
control thread. Stream retirement retains the existing quarantine-on-unsafe-stop
policy. Reopen attempts use 100 ms exponential backoff capped at five seconds;
only a fresh preference-free startup may select the default. A missing remembered
output reports `reconnecting`, never a substitute. Loss reports `device-lost`
before retrying; failed enumeration does not retire a healthy stream. Device-free regressions link a
fake implementation of the existing audio service boundary and advance poll time
explicitly; production callbacks perform no recovery, enumeration or allocation.
