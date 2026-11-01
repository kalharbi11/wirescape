<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ENGINE_PROTOCOL — third-pass Qt boundary audit and proposed web-host protocol

**Status:** research proposal only; no architecture decision or implementation approval. **Baseline:** Git tree 6e3c253550428f2bb964af4434bd429b5284cce2 (main, inspected 2026-11-01). **Scope:** current Qt Quick bridge, native document format, C++ app facade/audio/MIDI paths, checked-in Studio HTML sketch. **Method:** source inspection only; no build, live device, LAN test, timing benchmark, or attached external sketch. References are repository-relative file:line. Estimates and protocol numbers below are **proposed budgets**, not measurements.

## Executive findings: facts versus recommendations

**Verified facts**
1. One QObject, StudioController, is exposed to QML as context name studio; studioFontFamily is a separate string context value. No qmlRegisterType/qmlRegisterSingleton use was found in the scoped code search. It declares **24 properties, 23 Q_INVOKABLE methods and four signals**, and QML invokes most methods synchronously from the GUI thread (native/ui/main.cpp:53-62; native/ui/studio_controller.h:17-114).
2. QTimer drives 50 ms / **20 Hz** status/meter notification, independent of the PortAudio callback (native/ui/studio_controller.cpp:91-103). Callback and stop/quarantine paths are in native/ui/audio_device.cpp:102-115,145-199.
3. The QtCore editor/document library is optional and the app/runtime engine is not linked to it; the Qt-free runtime_patch.cpp uses nlohmann/json and builds AppEngine (native/document/CMakeLists.txt:8-32; native/document/runtime_patch.cpp:28-78; native/control/app_engine.h:5-41).
4. Qt presents an editor projection with native-v1-shaped numeric parameter values and audio/control/event cable labels. Canonical saved v2 has stable string parameter IDs, signal/modulation/event cables, outputs and optional state. Translation is in native/document/app_patch.cpp:9-48; canonical keys are in docs/specs/patch_v2_draft.schema.json:6-110.
5. Important current UI constraints are **not** the full ABI2 contract: its editor validator rejects cycles, self-edges, multiple cables into any input, and unequal port kinds; the ABI2 specification instead permits feedback and multiple event sources and treats signal hints as non-gating (native/ui/studio_controller.cpp:237-262; docs/specs/ABI2_SPEC.md:17-25,63-71).
6. The checked-in prototype is **visual only**: three visual types (chaos/modulator/interface), node-N string IDs, synthetic wave and meters, values 0..100, and a different persisted format (docs/design/STUDIO_USER_SKETCH.html:191-220,245-260,299-305). Do not directly pass its exported JSON to the native patch loader.

**Recommendation:** use canonical patch v2 and stable module/port/parameter identity as the source of truth; place a thin, Qt-free serial control host between the engine and web front end. Retain the sketch's design, interactions and layout as web presentation, but label visual-only affordances until native services exist. This is a proposed design, not evidence that such a host has been implemented.

## 1. Complete QML exposure inventory

**Thread legend:** G = Qt GUI/main event loop, where the QObject is constructed; A = PortAudio realtime callback (not a QML method); M = MIDI backend/service thread(s). QML getters and Q_INVOKABLE methods run synchronously on G for today's direct calls; the listed native side effects can interact with A/M through queues or stop/join paths (native/ui/main.cpp:18-20,53-62,102; native/ui/studio_controller.cpp:52-103,362-401; native/ui/audio_device.cpp:173-199; native/io/midi/midi_service.cpp:33-65). Property table args are all none. h = declaration in native/ui/studio_controller.h, implementation facts in native/ui/studio_controller.cpp unless specified. **No QML use** means exposed but not called by checked-in QML.

### Properties (24)

| Name / declaration | Kind; return | QML consumer file:line | Thread | Read side effect / semantics |
|---|---|---|---|---|
| midiCounters — native/ui/studio_controller.h:19 | property; QVariantMap | native/ui/qml/Main.qml:168-177 | G | parses MIDI diagnostic JSON; notify metersChanged (native/ui/studio_controller.cpp:627) |
| midiEndpoints — native/ui/studio_controller.h:20 | property; QVariantList | native/ui/qml/Main.qml:179-184 | G | **may enumerate** backend endpoints when stopped; while running shows saved enabled selection (native/ui/studio_controller.cpp:579-590) |
| midiAuthorized — native/ui/studio_controller.h:21 | property; bool | native/ui/qml/Main.qml:175,183 | G | reads MIDI authorization (native/ui/studio_controller.h:63) |
| midiSettingsAvailable — native/ui/studio_controller.h:22 | constant property; bool | native/ui/qml/Main.qml:175-176 | G | checks presence of QSettings (native/ui/studio_controller.h:64) |
| modules — native/ui/studio_controller.h:23 | property; QVariantList | native/ui/qml/Main.qml:15,404 | G | constructs descriptor/parameter/port projection; unresolved ports synthesized from saved cables (native/ui/studio_controller.cpp:145-175) |
| connections — native/ui/studio_controller.h:24 | property; QVariantList | native/ui/qml/Main.qml:16,360,423 | G | synthesizes channels for cable rendering via propagation (native/ui/studio_controller.cpp:177-205) |
| palette — native/ui/studio_controller.h:25 | constant property; QVariantList | native/ui/qml/Main.qml:256 | G | enumerates loaded catalog (native/ui/studio_controller.cpp:138-143) |
| running — native/ui/studio_controller.h:26 | property; bool | native/ui/qml/Main.qml:49,104,115,164,175,183,187,305,314,425,445,449,471; native/ui/qml/ModuleCard.qml:34,145 | G | reads AudioDevice state (native/ui/studio_controller.h:71) |
| ready — native/ui/studio_controller.h:27 | property; bool | native/ui/qml/Main.qml:314 | G | prepared engine flag (native/ui/studio_controller.h:72) |
| cleanupPending — native/ui/studio_controller.h:28 | property; bool | native/ui/qml/Main.qml:314 | G | stream exists but not running (native/ui/studio_controller.h:73) |
| statusText — native/ui/studio_controller.h:29 | property; QString | native/ui/qml/Main.qml:472 | G | formats three-state playback string (native/ui/studio_controller.cpp:206) |
| errorText — native/ui/studio_controller.h:30 | property; QString | native/ui/qml/Main.qml:94,472 | G | last error; statusChanged notification (native/ui/studio_controller.cpp:207) |
| noticeText — native/ui/studio_controller.h:31 | property; QString | native/ui/qml/Main.qml:472 | G | transient render/save notice (native/ui/studio_controller.cpp:536-539) |
| diagnosticText — native/ui/studio_controller.h:32 | property; QString | native/ui/qml/Main.qml:473 | G | reads engine meter and callback faults (native/ui/studio_controller.cpp:468-472) |
| outputNode — native/ui/studio_controller.h:33 | property; int | native/ui/qml/ModuleCard.qml:144-145 | G | reads editor document outputNode (native/ui/studio_controller.h:80) |
| peak — native/ui/studio_controller.h:34 | property; double | native/ui/qml/Main.qml:462 | G | cached peak updated on 50ms timer (native/ui/studio_controller.cpp:99-100) |
| cpuLoad — native/ui/studio_controller.h:35 | property; double | native/ui/qml/Main.qml:473 | G | reads callback atomic load x100 (native/ui/studio_controller.h:82; native/ui/audio_device.cpp:99-100) |
| underruns — native/ui/studio_controller.h:36 | property; int | native/ui/qml/Main.qml:473 | G | casts callback atomic counter to int (native/ui/studio_controller.h:83) |
| dirty — native/ui/studio_controller.h:37 | property; bool | native/ui/qml/Main.qml:12,60,76,304 | G | save/discard guard (native/ui/studio_controller.h:84) |
| patchName — native/ui/studio_controller.h:38 | property; QString | native/ui/qml/Main.qml:12,304 | G | display name based on saved/loaded filename (native/ui/studio_controller.cpp:449,520) |
| devices — native/ui/studio_controller.h:39 | property; QVariantList | native/ui/qml/Main.qml:449-452 | G | enumerates initialized PortAudio outputs on **property read** (native/ui/audio_device.cpp:85-96) |
| selectedDevice — native/ui/studio_controller.h:40 | property; int | native/ui/qml/Main.qml:452 | G | selected index, -1 meaning system default (native/ui/studio_controller.h:87; native/ui/audio_device.cpp:85-95) |
| sampleRate — native/ui/studio_controller.h:41 | constant property; int | native/ui/qml/Main.qml:473 | G | hardcoded 48000 (native/ui/studio_controller.h:88) |
| blockSize — native/ui/studio_controller.h:42 | constant property; int | native/ui/qml/Main.qml:473 | G | hardcoded 256 (native/ui/studio_controller.h:89) |

### Invokable commands (23)

| Name, args -> return; declaration | QML caller file:line | Thread | Current effect and implementation evidence |
|---|---|---|---|
| setMidiAuthorized(bool) -> void; native/ui/studio_controller.h:65 | native/ui/qml/Main.qml:175 | G | denied in offline/probe or while active; updates MIDI consent and QSettings, rollback on failed sync (native/ui/studio_controller.cpp:591-616) |
| setMidiEndpoint(QString name, QString system, bool enabled) -> void; native/ui/studio_controller.h:66 | native/ui/qml/Main.qml:184 | G | selection changes only stopped/authorized; marks dirty (native/ui/studio_controller.cpp:618-625) |
| refreshMidi() -> void; native/ui/studio_controller.h:67 | native/ui/qml/Main.qml:187 | G | triggers statusChanged when idle; endpoint getter does enumeration (native/ui/studio_controller.cpp:579-590,617) |
| diagnostics() -> QVariantMap; native/ui/studio_controller.h:78 | no QML use | G | composes device, patch, engine overload/queue stats and UTC time (native/ui/studio_controller.cpp:452-467) |
| saveDiagnostics(QUrl) -> bool; native/ui/studio_controller.h:79 | no QML use | G | atomic JSON write and notice/error (native/ui/studio_controller.cpp:473-478) |
| setParameter(int node,int parameter,double value) -> bool; native/ui/studio_controller.h:90 | native/ui/qml/ParameterControl.qml:52,194 | G | range/finite validation, running-engine enqueue, document mutation, dirty and parameterChanged (native/ui/studio_controller.cpp:310-317) |
| moveModule(int node,double x,double y) -> void; native/ui/studio_controller.h:91 | native/ui/qml/ModuleCard.qml:28 | G | finite positions persist on editor projection, marks dirty (native/ui/studio_controller.cpp:319-323) |
| addModule(QString id) -> void; native/ui/studio_controller.h:92 | native/ui/qml/Main.qml:278 | G | smallest unused positive ID; default parameter values; quiescent rebuild (native/ui/studio_controller.cpp:325-333,298-307) |
| removeModule(int node) -> void; native/ui/studio_controller.h:93 | native/ui/qml/ModuleCard.qml:35 | G | removes node/attached cables; clears output if selected; rebuild (native/ui/studio_controller.cpp:335-340) |
| connectPorts(int from,QString port,int to,QString input) -> void; native/ui/studio_controller.h:94 | native/ui/qml/Main.qml:111 | G | validates matching port kind, creates cable, quiescent rebuild (native/ui/studio_controller.cpp:342-358) |
| disconnectCable(int index) -> void; native/ui/studio_controller.h:95 | native/ui/qml/Main.qml:425 | G | removes current **array index**, rebuild; no stable cable handle exposed (native/ui/studio_controller.cpp:360) |
| startAudio() -> void; native/ui/studio_controller.h:96 | native/ui/qml/Main.qml:314 | G | recompile, init/open/start PortAudio, then start MIDI; defaults 48k/256 (native/ui/studio_controller.cpp:381-399) |
| stopAudio() -> void; native/ui/studio_controller.h:97 | native/ui/qml/Main.qml:314 | G | stop MIDI, stop/close audio and reset only after detach; pending cleanup retained (native/ui/studio_controller.cpp:401; native/ui/audio_device.cpp:145-161) |
| setGate(bool on) -> void; native/ui/studio_controller.h:98 | native/ui/qml/Main.qml:35,164,218,226,436 | G | if running, sends gate to nodes until rejection; control queue (native/ui/studio_controller.cpp:402,362-376) |
| note(int pitch,bool on) -> void; native/ui/studio_controller.h:99 | native/ui/qml/Main.qml:36,218,226 | G | 0..127; route to host.keyboard if present, else live nodes; queue note with release semantics (native/ui/studio_controller.cpp:403-415) |
| panic() -> void; native/ui/studio_controller.h:100 | native/ui/qml/Main.qml:164,437 | G | live panic vs stopped reset, updates generation; statusChanged (native/ui/studio_controller.cpp:416-425) |
| setDevice(int id) -> void; native/ui/studio_controller.h:101 | native/ui/qml/Main.qml:456 | G | updates preferred PortAudio **index** when stopped only (native/ui/studio_controller.cpp:379) |
| refreshDevices() -> void; native/ui/studio_controller.h:102 | native/ui/qml/Main.qml:445 | G | reinitializes PortAudio while stopped/clean (native/ui/studio_controller.cpp:378; native/ui/audio_device.cpp:66-76) |
| selectOutput(int node) -> void; native/ui/studio_controller.h:103 | native/ui/qml/ModuleCard.qml:146 | G | sets editor outputNode and rebuilds (native/ui/studio_controller.cpp:428) |
| savePatch(QUrl) -> bool; native/ui/studio_controller.h:104 | native/ui/qml/Main.qml:65,193 | G | v2 conversion, MIDI selections, atomic save/v1 backup, update source and clear dirty (native/ui/studio_controller.cpp:430-450; native/document/patch_document.cpp:220-267) |
| loadPatch(QUrl) -> bool; native/ui/studio_controller.h:105 | native/ui/qml/Main.qml:199 | G | max 2MiB read/inspect, v1/v2 read, editor validation, engine prepare, MIDI restore, atomic swap on success (native/ui/studio_controller.cpp:480-521) |
| renderDemo(QUrl) -> void; native/ui/studio_controller.h:106 | native/ui/qml/Main.qml:204 | G | distinct offline engine, fixed 4s/192000-frame mono PCM16 WAV, 2s gate; **384044-byte** PCM WAV, non-overwrite (native/ui/studio_controller.cpp:523-539) |
| resetPatch() -> void; native/ui/studio_controller.h:107 | native/ui/qml/Main.qml:71 | G | resets default 5-node/5-cable patch, MIDI selection and dirty, rebuild (native/ui/studio_controller.cpp:23-43,426) |

### Signals and other QML-facing interfaces

| Name | Kind/arguments/return | QML consumer file:line | Thread, side effect |
|---|---|---|---|
| documentChanged() | signal; none -> void (native/ui/studio_controller.h:111) | implicit model binding in native/ui/qml/Main.qml:15-16,360,404 and native/ui/qml/ModuleCard.qml:144 | emitted on G following topology/load/reset (native/ui/studio_controller.cpp:306-307,426,520) |
| parameterChanged(int node,int parameter,double value,QString display) | signal -> void (native/ui/studio_controller.h:112) | native/ui/qml/ParameterControl.qml:196-207 | emitted on G after accepted editor parameter mutation (native/ui/studio_controller.cpp:316) |
| statusChanged() | signal; none -> void (native/ui/studio_controller.h:113) | native/ui/qml/Main.qml:47-50 and implicit status bindings | emitted on G after errors/device/MIDI/running/save/status changes (native/ui/studio_controller.cpp:207,316,379,398,401,615,624) |
| metersChanged() | signal; none -> void (native/ui/studio_controller.h:114) | implicit numeric/midi bindings in native/ui/qml/Main.qml:168-177,462,473 | 50ms G timer, not A callback (native/ui/studio_controller.cpp:91-103) |
| studio | QQmlContext object pointer (native/ui/main.cpp:59) | all listed uses | G, only C++ service directly available to QML |
| studioFontFamily | QQmlContext QString (native/ui/main.cpp:60) | native/ui/qml/Main.qml:14; native/ui/qml/ParameterControl.qml:66,75; native/ui/qml/ModuleCard.qml:20-21 | presentation string, no engine effects |

**Non-QML public helpers:** smokeCheck(QJsonObject&) and keyboardChordCheck(QJsonObject&) are test probes, not Q_INVOKABLE or user commands (native/ui/studio_controller.h:108-109; native/ui/studio_controller.cpp:541-577). Startup CLI flags (including --smoke-test, --ready-report, --window-size, --fullscreen, --confirm-midi) belong to shell/startup administration rather than patch messages (native/ui/main.cpp:29-55,64-100). The rendering, geometry, zoom, selected node/cable, focus, unsaved dialog and keyboard shortcut state live in QML and should remain web-local except when changing persisted patch positions or dispatching commands (native/ui/qml/Main.qml:17-31,51-76,97-162,206-228,351-426).

## 2. Qt dependency inventory outside native/ui/

**Production / reusable code; each row cites the dependency and estimates active engineering effort, not benchmark time.**

| Source file:line | Qt surface / effect | Blocks a Qt-free host? | Estimated removal |
|---|---|---|---|
| native/document/document_reader.h:2-5,15-35; native/document/document_reader.cpp:2-5,34-102 | QByteArray, QJsonObject/Value, QString, QVector/QSet; format inspection and retained bytes | **Yes for this inspector API**, **no for existing headless engine** | 0.5–1.5 engineer-days to port + test duplicate/unknown behavior |
| native/document/patch_document.h:4-28 | QHash<QString,...>, QJsonObject, QByteArray, QString public catalog/document/save API | **Yes for lossless editor document API**; independent runtime loader exists | 0.5–1 engineer-day interfaces + adapters |
| native/document/patch_document.cpp:3-11,29-64,68-142,145-267 | Qt JSON parser/objects, QSet/QVector duplicate detection, QFile/QTemporaryFile/QSaveFile, QDir/QFileInfo, v1 migration, validation, backup and atomic save | **Yes for production-equivalent load/save/migration**, not for DSP | 2–4 engineer-days with fault injection/atomicity equivalence tests |
| native/document/app_patch.h:7-9; native/document/app_patch.cpp:4-5,9-54 | Qt JSON projection, stable-ID conversion, editor view, app graph construction | **Yes for current UI projection**, no for raw v2 runtime loader | 1–2 engineer-days with unmapped metadata tests |
| native/document/CMakeLists.txt:20-32,33-66 | optional Qt6::Core target; only builds wirescape_document when Qt exists | **No** to headless core; **yes** to current document target | 0.25–0.5 day CMake/packaging refactor |
| native/engine2/CMakeLists.txt:172-183; native/engine2/tests/soak.cpp:19-22,97-98 | optional Qt-linked document soak variant and conditional QFile | **No** to engine2/normal soak; test wiring only | 0.25–0.5 day |
| native/document/app_patch_tests.cpp:5-11; native/tests/document_reader_tests.cpp:2-5; native/tests/patch_document_tests.cpp:3-8 | QtCore test fixtures, JSON and filesystem helpers | **No** runtime blocker; migration tests must be retained | 1–2 engineer-days across tests |
| native/tests/patch_render_tests.cpp:5-11; native/tests/golden_render.cpp:4-10 | QtCore in tests/hash reference render | **No** runtime blocker; conversion may affect golden determinism | 0.5–1 day across tests |

**Non-dependencies established by scoped source inspection:** native/control/app_engine.h:1-41 and native/control/app_engine.cpp:1-119 use C++/engine2 only; native/control/midi_host.cpp:1-126 and native/io/midi/midi_service.cpp:1-139 do not include Qt; native/io/audio/portaudio_service.cpp:1-96 and native/engine2/engine2.cpp:1-745 likewise do not use Qt. native/document/runtime_patch.cpp:1-79 uses nlohmann/json and has **no QtCore dependency**. native/CMakeLists.txt:17-21,99-110 makes UI optional. Search results for QJson/QFile/QString/Qt6::Core were also checked to cover Qt-using test targets. **Do not infer this inventory is a proof against every possible indirect dependency or linked third-party dynamic library:** validate a real -DWIRESCAPE_BUILD_UI=OFF build and dependency scan in CI.

**Total provisional Qt-document extraction:** about **5–10 engineer-days** plus CI/platform stabilization. Cheapest compatible transition: reuse the Qt-free runtime reader initially; do not advertise fully lossless GUI save/load until the Qt document writer's tests have equivalent non-Qt coverage.

## 3. Design hazards specifically exposed by the UI switch

| Priority | Observed behavior | Required design response |
|---|---|---|
| P0, patch fidelity | editor_view projects saved v2 into a synthetic version=1 editor object; editor_patch maps v1-style values back to stable IDs (native/document/app_patch.cpp:16-48). | Never serialize the web snapshot as canonical patch; keep an authoritative v2 document and lossless unrecognized fields. |
| P0, incompatible v2 import | UI validateDocument rejects feedback and multiple connections to event inputs and only accepts audio/control/event and equal port kinds (native/ui/studio_controller.cpp:212-262); v2 schema admits modulation/feedback (docs/specs/patch_v2_draft.schema.json:55-90). | Use native patch v2 validation + runtime preflight as authority, not the Qt editor validator. Distinguish signal hint from cable kind. |
| P0, potential stereo/output loss | when outputNode changes, editor_patch replaces the outputs collection with only bus channel 0 (native/document/app_patch.cpp:45-47). | Expose outputs as an array; selecting output must not silently remove bus 1 or other routes. Explicit output-routing command and regression test. |
| P0, lifetime | stop()/detachEngine() can fail; stream still borrows context/engine, so destruction is quarantined rather than freed (native/ui/audio_device.cpp:11-23,102-115,145-161; native/ui/studio_controller.cpp:106-111). | Represent CleanupPending as first-class lifecycle state; never free/reset borrowed engine until all callback owners quiesce. |
| P1, transactional editing | topology edit rebuilds engine only if stopped; failed edits leave old document/engine unchanged; parameter edit is an enqueue while running (native/ui/studio_controller.cpp:298-317,362-375). | Require expectedRevision and reply with commit status; differentiate queued from applied and keep graph rebuild off callback. |
| P1, indices | current deletion is by cable array index and device selection by PortAudio index; node IDs are numeric and add reuses smallest gap (native/ui/studio_controller.cpp:325-332,360,379; native/ui/audio_device.cpp:85-96). | Expose session-scoped cable key and device list epoch; reject stale selections. Never renumber existing module IDs. |
| P1, capability gap | Qt exposes 20Hz peak/cpu/xruns but no scope waveform; prototype's SVG wave and audio meters are illustrative (native/ui/studio_controller.cpp:91-100; docs/design/STUDIO_USER_SKETCH.html:229-248). | Offer real telemetry capability flags; a visual preview cannot be labelled as live metering. |
| P1, format collision | prototype exports visual.study v4 with node-N IDs; native read_patch accepts only wirescape.native.patch v1/v2 (docs/design/STUDIO_USER_SKETCH.html:210-224,299-305; native/document/patch_document.cpp:145-151). | Explicit **separate** sketch importer with preview and mapping decisions; never auto-coerce unknown IDs, controls or port kinds. |
| P1, file privilege | QML passes QUrl to QFile/QSaveFile; non-local URL strings can fall through to path handling (native/ui/studio_controller.cpp:430-431,473-475,480-483,523-525). | Web client must not supply arbitrary host paths or file:// URLs. Use native dialog-issued short-lived opaque handles or explicit bounded import/export bytes. |
| P2, presentation | Qt has a module descriptor-driven UI; HTML hardcodes three visual types, 0–100 knobs, modes and cable rules (native/ui/studio_controller.cpp:145-172; docs/design/STUDIO_USER_SKETCH.html:191-203,252-265,274-283). | Preserve the visual composition, but bind controls to actual descriptor stable IDs/ranges/tapers and signal metadata. Unsupported modes remain explicitly design-only. |

## 4. Proposed transport, state machine and threading

**Transport A — loopback WebSocket** for an ordinary browser/dev build. Bind only 127.0.0.1 and ::1, random OS-chosen port; launch generates >=128-bit random bearer token and passes it through a native launch handshake, **not** URL query parameters or persisted localStorage. Reject missing/unknown Origin, unauthenticated WebSocket upgrade, cross-site requests, non-loopback bind, oversized frames and duplicate JSON keys. No LAN listener. Browser-only development needs a secure way to obtain token (local launcher/temporary page), not a hardcoded token. Rate-limit unauthenticated handshakes; revoke token on host restart.

**Transport B — Tauri IPC bridge**: front end invokes an allowlisted Rust/Tauri command that delivers the **same protocol envelope** to a C++ host thread or bounded sidecar pipe; native event channel delivers notifications. No open localhost listener required, smaller network attack surface, but Rust glue/sidecar lifecycle and event ordering require tests. **Recommendation:** Tauri IPC in packaged Windows/Pi builds, optional tokenized loopback WS in developer browser mode; never maintain two business-logic implementations. Transport selection is a planner choice, not a source-established requirement.

**Separation of ownership:** UI owns camera/pan/zoom, hover, selection, pointer animations, keyboard focus and dialogs. Host owns canonical patch v2 and its monotonic document revision, descriptor catalog, parameter truth, audio/MIDI lifecycle, devices, diagnostics and application-generation acknowledgments. One authenticated controlling session at a time for v1; other viewers read-only, or reject until arbitration is designed. No automatic audio start on reconnect.

**Threads and data flow (proposed):**
1. Transport I/O (T): bound/max-size/parse/authenticate JSON; place an immutable command on bounded mailbox to control thread C. Never hold engine pointers and never invoke audio render.
2. Control/host serial executor (C): validate stable IDs, permissions, expectedRevision and limits; perform catalog, patch, graph construction, PortAudio/MIDI start/stop orchestration **quiescently**, and publish immutable state snapshots. Disk worker (D) performs bounded file reads, temp backups and writes; commit prepared state on C only after validation. Do not change graph while active in first iteration.
3. A = PortAudio realtime callback: preallocated scratch; only prepared engine render and bounded atomic counters. No JSON, sockets, file I/O, Qt, malloc/new, mutex, logging or exceptions. MIDI service threads (M) feed bounded existing rings; source identity and note-offs have priority. Current control enqueue uses app_engine_enqueue on producer 0 (native/control/app_engine.cpp:72-89), and MIDI ring push is bounded (native/io/midi/midi_service.cpp:11-20,33-65).
4. Telemetry sampler (S), non-RT: reads published atomics/meter snapshots at 30Hz by default, opt-in 60Hz; formats JSON and sends latest-only through T. Current meter read on 50ms Qt timer is already off A (native/ui/studio_controller.cpp:91-103). **No audio buffer copies or serialization on A**; future scope must be copied through an allocation-free bounded SPSC tap with samples downsampled on S. Scope is **not implemented today**.
5. Audio stopping: C first prevents new producers, stops MIDI, disables callback owner, closes/drains PortAudio, checks inFlight==0, then resets/disposes engine; on timeout remain cleanup_pending and deny rebuild/restart (native/ui/audio_device.cpp:102-115,145-161). No timeout can be treated as proof of quiescence.

**Proof obligations, not yet proven implementation:** static ownership graph above excludes transport/JSON/file code from the A call chain. The present A chain is native/ui/audio_device.cpp:173-199 -> native/ui/studio_controller.cpp:385-391 -> native/control/app_engine.cpp:91-100 -> engine2::Engine::render. The separate Qt-free headless audio callback is native/io/audio/portaudio_service.cpp:54-67. Both need allocation-watch + fault-injection tests on exact shipped host head, because review alone cannot prove absence of dynamic allocation in every module. In particular, current UI callback has a try/catch at native/ui/audio_device.cpp:185-187; a replacement must ensure native callbacks never propagate exceptions and must not add throwing/allocating work. Existing live callbacks differ in output: Qt bridge copies mono to stereo (native/ui/audio_device.cpp:191-194), headless service renders two planar output channels (native/io/audio/portaudio_service.cpp:64-66). **Do not change audible routing accidentally.**

**Command lifecycle:** authenticated -> structurally validated -> authorized -> optimistic concurrency check -> accepted/queued (reply) -> applied/committed (state event). An audio/MIDI enqueue reply saying accepted must **not** imply that the callback rendered it; publish generation/applied-frame on telemetry when observable. Topology mutation must reply only after prepared candidate and document have committed; on failure leave both old snapshot and revision untouched. Only C mutates host state. On transport disconnect, synthesize note-off/panic for web-held keys through reserved critical control path, then revoke control ownership; keep audio running/stopping policy explicit and configurable. The QML precedent releases held notes on blur and status loss (native/ui/qml/Main.qml:32-50,164,210-228).

## 5. Wire envelopes and every inventory operation

All messages UTF-8 JSON objects, protocol **v=1**, explicit type. Commands carry a unique string request id, op, args and (for durable edits) an expectedRevision decimal string. Replies echo id/op and contain ok plus result **or** a structured error. Events carry topic, strictly increasing per-session seq (decimal string), current revision, and typed data. uint64 frames, generation, counters and seq are serialized as decimal strings to avoid JavaScript's 2^53 integer rounding; node IDs remain uint32 numeric. A schema is embedded below.

**Example (proposed):**
- command: {"v":1,"type":"command","id":"r-42","op":"parameter.set","args":{"nodeId":2,"parameterId":"frequency","value":440},"expectedRevision":"17"}
- reply: {"v":1,"type":"reply","id":"r-42","op":"parameter.set","ok":true,"result":{"accepted":true,"revision":"18","display":"440.0 Hz"}}
- event: {"v":1,"type":"event","topic":"parameter.changed","seq":"203","revision":"18","data":{"nodeId":2,"parameterId":"frequency","value":440,"display":"440.0 Hz"}}
- rejection: {"v":1,"type":"reply","id":"r-42","op":"parameter.set","ok":false,"error":{"code":"CONFLICT","message":"Patch revision changed","retryable":true,"details":{"currentRevision":"19"}}}

### Operation mapping — exactly the 23 current invokables plus supporting web operations

| Qt API -> proposed op | args (all JSON) | success result / event; effect |
|---|---|---|
| setMidiAuthorized -> midi.authorize | enabled:bool | accepted; status.changed; persists user consent, stopped only |
| setMidiEndpoint -> midi.endpoint.set | name:string, systemName:string, enabled:bool | accepted+revision; status.changed/document.changed |
| refreshMidi -> midi.refresh | {} | accepted; status.changed with new endpoints |
| diagnostics -> diagnostics.get | {} | diagnostics object; read-only |
| saveDiagnostics -> diagnostics.save | fileHandle:string | accepted; status.changed notice |
| setParameter -> parameter.set | nodeId:u32, parameterId:stable string, value:finite number | accepted+revision+display; parameter.changed, status.changed; coalesced UI submission |
| moveModule -> module.move | nodeId:u32, x:finite, y:finite | accepted+revision; document.changed or layout delta |
| addModule -> module.add | moduleType:stable string | accepted+nodeId+revision; document.changed |
| removeModule -> module.remove | nodeId:u32 | accepted+revision; document.changed |
| connectPorts -> connection.add | kind:signal/modulation/event, from:endpoint, to:endpoint; amount for modulation; optional feedback | accepted+connectionKey+revision; document.changed |
| disconnectCable -> connection.remove | connectionKey:string, expectedRevision | accepted+revision; document.changed (key from current snapshot, never raw index) |
| startAudio -> audio.start | {} | accepted+status; status.changed; no automatic restart |
| stopAudio -> audio.stop | {} | accepted+status; status.changed; may report cleanup_pending |
| setGate -> keyboard.gate | on:bool | accepted; controlled gate, critical off |
| note -> keyboard.note | pitch:int 0..127, on:bool | accepted; note-off critical, never coalesced/dropped silently |
| panic -> keyboard.panic | {} | accepted; status.changed; bypass normal knob backlog |
| setDevice -> audio.device.select | deviceId:opaque string, devicesEpoch:string | accepted; status.changed; stopped only |
| refreshDevices -> audio.devices.refresh | {} | accepted+devicesEpoch; status.changed |
| selectOutput -> patch.output.select | nodeId:u32, optional portId:stable string, busChannel:int 0..15 | accepted+revision; document.changed; preserve all other outputs |
| savePatch -> patch.save | fileHandle:string | accepted+revision; status.changed; lossless v2 saved |
| loadPatch -> patch.load | fileHandle:string | accepted+revision; document.changed/status.changed/meters.changed |
| renderDemo -> render.demo | fileHandle:string | accepted; status.changed notice; offline WAV only |
| resetPatch -> patch.reset | {} | accepted+revision; document.changed/status.changed |
| new: session.hello | minVersion:int, maxVersion:int | negotiatedVersion, capabilities, sessionId; rejects incompatible client |
| new: snapshot.get | {} | full state (all 24 Q_PROPERTY-equivalent values and host revision) |
| new: catalog.get | {} | palette + stable parameter/port descriptor metadata |
| new: file.pick | purpose:openPatch/savePatch/saveWav/saveDiagnostics | opaque fileHandle and displayName; native modal picker on Tauri shell |
| new: patch.import | jsonText:UTF-8 source JSON text | accepted+revision; same transactional loader, 2MiB byte cap, no raw path |
| new: patch.export | {} | jsonText canonical v2, suggested filename; no filesystem access |
| new: telemetry.subscribe | stream:meters/scope, hz:30/60; scope target if supported | subscriptionId; meters.changed or scope.chunk |
| new: telemetry.unsubscribe | subscriptionId:string | accepted; no more sample events |

**Snapshot properties:** midiCounters, midiEndpoints, midiAuthorized, midiSettingsAvailable, modules, connections, palette, running, ready, cleanupPending, statusText, errorText, noticeText, diagnosticText, outputNode, peak, cpuLoad, underruns, dirty, patchName, devices, selectedDevice, sampleRate and blockSize. Supplement with documentRevision, devicesEpoch, engineGeneration, capabilities and audioState. Each is a read-only value in the web model, never an independent browser-owned truth. Parameter descriptor supplies both stable parameterId for protocol and legacy numeric index for any compatibility view; never convert an unknown stable ID to guessed index. Events replace Qt NOTIFY: document.changed -> documentChanged; parameter.changed -> parameterChanged; status.changed -> statusChanged; meters.changed -> metersChanged. Session and scope events are additions, not existing Qt capabilities.

**Wire caveats:** no QML-level return exists for many void invokables, but the new protocol returns errors for **every** command. All mutating commands use request id idempotency cache (bounded per session; duplicate id+same payload returns prior result, reused id+different payload is INVALID_ARGUMENT). Include expectedRevision on patch, module, connection and parameter mutations; a stale write is rejected. Volatile gate/note/panic/audio stop do not need document revision. Apply server-side input validation even if JSON schema passed. Telemetry seq gaps are permitted and mean samples were dropped; durable events/replies never use a drop-oldest path.

### Errors and limits

| code | Meaning | retryable? / handling |
|---|---|---|
| INVALID_MESSAGE / UNSUPPORTED_VERSION | bad JSON, unknown op, malformed envelope, no compatible protocol | no; fail before command dispatch |
| UNAUTHORIZED / FORBIDDEN | bad token/Origin, read-only session, missing MIDI consent | no; do not leak file/device data |
| INVALID_ARGUMENT / NOT_FOUND | missing node/port/parameter, out-of-range/non-finite value, bad file handle | no; include safe field-level detail |
| CONFLICT | expectedRevision or devicesEpoch stale | yes after snapshot/device refresh |
| BUSY / AUDIO_RUNNING / CLEANUP_PENDING | forbidden edit while audio active; uncertain callback ownership | retry only after explicit successful stop |
| QUEUE_FULL | live event enqueue could not accept control | parameter retry; for critical note-off escalate to panic/stop path |
| CAPABILITY_UNAVAILABLE | optional scope, unsupported device/backend/module | no until capability changes |
| PATCH_INVALID / PATCH_UNSUPPORTED | invalid/future format, duplicated JSON keys, oversized/invalid v2, unresolved migration | no; preserve original bytes and current patch |
| FILE_CANCELLED / IO_ERROR / DEVICE_ERROR / ENGINE_ERROR | dialog cancelled, disk/PortAudio/backend/preparation fault | cancellation not an error toast; retry only when state permits |
| INTERNAL | unexpected host failure | no blind retry; preserve prior authoritative state |

Bounds: **2 MiB original patch bytes** (existing native/document/document_reader.cpp:34-45 and native/document/patch_document.cpp:31-32,220-225), **64 modules/128 cables** in current runnable loader (native/document/runtime_patch.cpp:35-38), and engine2 physical runtime limits separately (docs/specs/ABI2_SPEC.md:89-92). Host must reject schema-valid but runtime-impossible documents without touching active graph. Proposed command cap 8 KiB except patch.import; proposed wire cap 4 MiB to allow JSON escaping/overhead. Disk writes must be same-directory atomic replace and keep a v1 companion backup prior to conversion as currently required (native/document/patch_document.cpp:220-267). Do not return raw filesystem paths to an untrusted browser.

### Backpressure at 30–60 Hz

S samples once per 33.3ms at 30Hz (or 16.7ms at explicit 60Hz). Bounded 1 outstanding meters frame **per subscriber** and 1 latest scope frame; drop/replace older unsent telemetry when transport backlog exceeds 64 KiB or 250ms, increment telemetryDropped. Close only the overrun subscription after sustained stalls, not the critical control channel. Retain max 64 events worth of critical session/revision notifications in a separate ordered queue; if durable subscriber falls behind, force re-snapshot rather than emit an incomplete diff. Prioritize reply, critical status/keyboard release, durable document events, then meters/scope. Never block A/T on a slow browser. UI knobs coalesce at most once per animation frame per node/parameter but always flush final pointer-up value; QML today submits directly on moved (native/ui/qml/ParameterControl.qml:50-56,98,137-140). For optional future real scope, sample preallocated SPSC tap and reduce to e.g. 128 min/max bins/channel/update on S, **not** on A; send only when feature capability real_scope=true. At 60Hz and two channels that is 15,360 min/max pairs/s (30,720 scalar values/s), subject to serialization overhead (design calculation, not measured throughput). Scope and the sketch's UI animation are separate concerns.

## 6. Patch v2 load/save and ID invariants

1. Canonical format=**wirescape.native.patch**, version=**2**. Required settings/modules/connections/outputs; modules use uint32 id, stable module string, version, config, **stable string parameter IDs** mapped to plain-unit numbers, optional versioned base64 state/preserved/inactive/channel_seed. Cables are signal/modulation/event with structured module/port endpoints, optional amount/feedback/stereo_pair; outputs have module, port, bus_channel; host_mappings/devices allowed. Unknown object fields remain allowed (docs/specs/patch_v2_draft.schema.json:6-110; docs/specs/patch_v2_draft.example.json:1-48).
2. On import, inspect raw bytes (max 2MiB), reject duplicate keys and unknown/future format, validate the v2 document, discover modules and stable IDs, stage complete engine with runtime parser and producer configuration, and commit only after prepare succeeds. Existing reader migrates native v1 by retaining the **whole old v1 JSON** in preserved plus per-node preserved payload and filling stable parameter IDs from descriptor mapping (native/document/patch_document.cpp:29-57,145-218; native/document/runtime_patch.cpp:28-75).
3. Missing modules must remain **editable and recoverable**; no partial execution of an unknown native DSP node. Current UI displays unresolved module and cable endpoints and disables ready status (native/ui/studio_controller.cpp:162-174,265-272,480-520). For v2, keep inactive, state and unrecognized fields untouched. Source formatting can change on v2 JSON serialization, but fields/data must remain semantically lossless; v1 original bytes must remain recoverable for backup.
4. Module IDs and stable descriptor port/parameter IDs never change. The sketch's node-N, modA, inL and normalized 0–100 UI values **are not** ABI IDs or parameter units (docs/design/STUDIO_USER_SKETCH.html:191-224,252-260; docs/specs/ABI2_SPEC.md:17-35). Mapping requires an explicit per-module UI adapter; unknown types/knobs are non-executable until mapped. Never silently use prototype JSON for patch.import. Provide a separate study.import preview requiring user-approved mapping later.
5. Store geometry as existing module x/y retained in canonical v2 unknown per-node fields; optionally persist namespaced ui.camera/theme metadata as an additive unknown top-level field after planner approval. Never move source of truth to localStorage. If saved v2 has multiple output buses/stereo channels, preserve every unedited entry; selectOutput cannot collapse them (native/document/app_patch.cpp:45-47).
6. Save should use a verified original document source, not a reconstructed snapshot; on failure it must not clear dirty or continue a New/Open/Close workflow. Current QML explicitly cancels continuation on save failure (native/ui/qml/Main.qml:51-94). Patch save uses v1 backup filename stem.v1.json and atomic QSaveFile; retain equivalent cross-platform semantics after Qt removal (native/document/patch_document.cpp:220-267). Retain no-overwrite behavior for WAV demo (native/ui/studio_controller.cpp:523-539).
7. Treat document setting sample_rate/feedback_block/max_channels and runtime output routing as part of patch fidelity, while displaying actual selected audio device configuration separately. Today Qt's status shows fixed 48k/256 even though runtime patch supports settings (native/ui/studio_controller.h:88-89; native/document/runtime_patch.cpp:39-46).
8. JSON Schema is structural only; additional native relational checks must enforce unique node IDs, existing endpoints, module descriptor compatibility, parameter bounds, accepted graph topology, available output routes, state versions, duplicate keys and byte limit (native/document/patch_document.cpp:86-142; native/document/runtime_patch.cpp:35-74). A schema validator **must not** be presented as complete patch validation.

## 7. JSON Schema — draft 2020-12, protocol v1

The following self-contained schema describes command/reply/event envelopes, all command argument shapes, snapshots, mandatory per-operation reply results and telemetry events. It does **not** replace the separate canonical patch-v2 schema at docs/specs/patch_v2_draft.schema.json:1-112. JSON Schema cannot enforce distinct JSON property names after parsing, token authentication, sequence monotonicity, finite numerical conversion on every runtime, filesystem safety, duplicate request IDs, cross-file presence of modules, real audio safety, or idempotent side effects. Those require host guards and tests.

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://wirescape.invalid/schema/engine-protocol-v1.json",
  "title": "Wirescape host/web protocol v1 — proposed",
  "description": "Structural wire validation only. Requires native authorization, lifecycle and patch relational validation.",
  "oneOf": [
    {
      "$ref": "#/$defs/command"
    },
    {
      "$ref": "#/$defs/reply"
    },
    {
      "$ref": "#/$defs/event"
    }
  ],
  "$defs": {
    "command": {
      "type": "object",
      "properties": {
        "v": {
          "const": 1
        },
        "type": {
          "const": "command"
        },
        "id": {
          "type": "string",
          "minLength": 1,
          "maxLength": 64
        },
        "op": {
          "enum": [
            "session.hello",
            "snapshot.get",
            "catalog.get",
            "file.pick",
            "patch.import",
            "patch.export",
            "telemetry.subscribe",
            "telemetry.unsubscribe",
            "midi.authorize",
            "midi.endpoint.set",
            "midi.refresh",
            "diagnostics.get",
            "diagnostics.save",
            "parameter.set",
            "module.move",
            "module.add",
            "module.remove",
            "connection.add",
            "connection.remove",
            "audio.start",
            "audio.stop",
            "keyboard.gate",
            "keyboard.note",
            "keyboard.panic",
            "audio.device.select",
            "audio.devices.refresh",
            "patch.output.select",
            "patch.save",
            "patch.load",
            "render.demo",
            "patch.reset"
          ]
        },
        "args": {
          "type": "object"
        },
        "expectedRevision": {
          "type": "string",
          "pattern": "^(0|[1-9][0-9]*)$",
          "maxLength": 20
        }
      },
      "required": [
        "v",
        "type",
        "id",
        "op",
        "args"
      ],
      "additionalProperties": false,
      "allOf": [
        {
          "if": {
            "properties": {
              "op": {
                "const": "session.hello"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/session_hello"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "snapshot.get"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/snapshot_get"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "catalog.get"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/catalog_get"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "file.pick"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/file_pick"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.import"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/patch_import"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.export"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/patch_export"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "telemetry.subscribe"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/telemetry_subscribe"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "telemetry.unsubscribe"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/telemetry_unsubscribe"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "midi.authorize"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/midi_authorize"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "midi.endpoint.set"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/midi_endpoint_set"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "midi.refresh"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/midi_refresh"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "diagnostics.get"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/diagnostics_get"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "diagnostics.save"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/diagnostics_save"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "parameter.set"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/parameter_set"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "module.move"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/module_move"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "module.add"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/module_add"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "module.remove"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/module_remove"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "connection.add"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/connection_add"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "connection.remove"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/connection_remove"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.start"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/audio_start"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.stop"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/audio_stop"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "keyboard.gate"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/keyboard_gate"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "keyboard.note"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/keyboard_note"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "keyboard.panic"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/keyboard_panic"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.device.select"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/audio_device_select"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.devices.refresh"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/audio_devices_refresh"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.output.select"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/patch_output_select"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.save"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/patch_save"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.load"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/patch_load"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "render.demo"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/render_demo"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.reset"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "$ref": "#/$defs/commandArgs/patch_reset"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "enum": [
                  "parameter.set",
                  "module.move",
                  "module.add",
                  "module.remove",
                  "connection.add",
                  "connection.remove",
                  "midi.endpoint.set",
                  "patch.output.select",
                  "patch.save",
                  "patch.load",
                  "patch.import",
                  "patch.reset"
                ]
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "required": [
              "expectedRevision"
            ]
          }
        }
      ]
    },
    "reply": {
      "type": "object",
      "properties": {
        "v": {
          "const": 1
        },
        "type": {
          "const": "reply"
        },
        "id": {
          "type": "string",
          "minLength": 1,
          "maxLength": 64
        },
        "op": {
          "enum": [
            "session.hello",
            "snapshot.get",
            "catalog.get",
            "file.pick",
            "patch.import",
            "patch.export",
            "telemetry.subscribe",
            "telemetry.unsubscribe",
            "midi.authorize",
            "midi.endpoint.set",
            "midi.refresh",
            "diagnostics.get",
            "diagnostics.save",
            "parameter.set",
            "module.move",
            "module.add",
            "module.remove",
            "connection.add",
            "connection.remove",
            "audio.start",
            "audio.stop",
            "keyboard.gate",
            "keyboard.note",
            "keyboard.panic",
            "audio.device.select",
            "audio.devices.refresh",
            "patch.output.select",
            "patch.save",
            "patch.load",
            "render.demo",
            "patch.reset"
          ]
        },
        "ok": {
          "type": "boolean"
        },
        "result": {
          "type": "object"
        },
        "error": {
          "$ref": "#/$defs/error"
        }
      },
      "required": [
        "v",
        "type",
        "id",
        "op",
        "ok"
      ],
      "additionalProperties": false,
      "oneOf": [
        {
          "properties": {
            "ok": {
              "const": true
            }
          },
          "required": [
            "result"
          ],
          "not": {
            "required": [
              "error"
            ]
          }
        },
        {
          "properties": {
            "ok": {
              "const": false
            }
          },
          "required": [
            "error"
          ],
          "not": {
            "required": [
              "result"
            ]
          }
        }
      ],
      "allOf": [
        {
          "if": {
            "properties": {
              "op": {
                "const": "session.hello"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/session_hello"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "snapshot.get"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/snapshot_get"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "catalog.get"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/catalog_get"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "file.pick"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/file_pick"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.import"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/patch_import"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.export"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/patch_export"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "telemetry.subscribe"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/telemetry_subscribe"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "telemetry.unsubscribe"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/telemetry_unsubscribe"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "midi.authorize"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/midi_authorize"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "midi.endpoint.set"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/midi_endpoint_set"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "midi.refresh"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/midi_refresh"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "diagnostics.get"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/diagnostics_get"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "diagnostics.save"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/diagnostics_save"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "parameter.set"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/parameter_set"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "module.move"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/module_move"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "module.add"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/module_add"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "module.remove"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/module_remove"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "connection.add"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/connection_add"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "connection.remove"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/connection_remove"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.start"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/audio_start"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.stop"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/audio_stop"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "keyboard.gate"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/keyboard_gate"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "keyboard.note"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/keyboard_note"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "keyboard.panic"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/keyboard_panic"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.device.select"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/audio_device_select"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "audio.devices.refresh"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/audio_devices_refresh"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.output.select"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/patch_output_select"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.save"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/patch_save"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.load"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/patch_load"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "render.demo"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/render_demo"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "patch.reset"
              },
              "ok": {
                "const": true
              }
            },
            "required": [
              "op",
              "ok"
            ]
          },
          "then": {
            "properties": {
              "result": {
                "$ref": "#/$defs/replyResults/patch_reset"
              }
            }
          }
        }
      ]
    },
    "event": {
      "type": "object",
      "properties": {
        "v": {
          "const": 1
        },
        "type": {
          "const": "event"
        },
        "topic": {
          "enum": [
            "document.changed",
            "parameter.changed",
            "status.changed",
            "meters.changed",
            "scope.chunk",
            "session.changed",
            "capabilities.changed"
          ]
        },
        "seq": {
          "type": "string",
          "pattern": "^(0|[1-9][0-9]*)$",
          "maxLength": 20
        },
        "revision": {
          "type": "string",
          "pattern": "^(0|[1-9][0-9]*)$",
          "maxLength": 20
        },
        "data": {
          "type": "object"
        }
      },
      "required": [
        "v",
        "type",
        "topic",
        "seq",
        "revision",
        "data"
      ],
      "additionalProperties": false,
      "allOf": [
        {
          "if": {
            "properties": {
              "topic": {
                "const": "document.changed"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/document_changed"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "topic": {
                "const": "parameter.changed"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/parameter_changed"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "topic": {
                "const": "status.changed"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/status_changed"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "topic": {
                "const": "meters.changed"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/meters_changed"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "topic": {
                "const": "scope.chunk"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/scope_chunk"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "topic": {
                "const": "session.changed"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/session_changed"
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "topic": {
                "const": "capabilities.changed"
              }
            },
            "required": [
              "topic"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "$ref": "#/$defs/eventData/capabilities_changed"
              }
            }
          }
        }
      ]
    },
    "error": {
      "type": "object",
      "properties": {
        "code": {
          "enum": [
            "INVALID_MESSAGE",
            "UNSUPPORTED_VERSION",
            "UNAUTHORIZED",
            "FORBIDDEN",
            "INVALID_ARGUMENT",
            "NOT_FOUND",
            "CONFLICT",
            "BUSY",
            "AUDIO_RUNNING",
            "CLEANUP_PENDING",
            "QUEUE_FULL",
            "CAPABILITY_UNAVAILABLE",
            "PATCH_INVALID",
            "PATCH_UNSUPPORTED",
            "FILE_CANCELLED",
            "IO_ERROR",
            "DEVICE_ERROR",
            "ENGINE_ERROR",
            "INTERNAL"
          ]
        },
        "message": {
          "type": "string",
          "maxLength": 2048
        },
        "retryable": {
          "type": "boolean"
        },
        "details": {
          "type": "object",
          "additionalProperties": true
        }
      },
      "required": [
        "code",
        "message",
        "retryable"
      ],
      "additionalProperties": false
    },
    "snapshot": {
      "type": "object",
      "properties": {
        "midiCounters": {
          "type": "object",
          "additionalProperties": true
        },
        "midiEndpoints": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "name": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "systemName": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "enabled": {
                "type": "boolean"
              }
            },
            "required": [
              "name",
              "systemName",
              "enabled"
            ],
            "additionalProperties": true
          }
        },
        "midiAuthorized": {
          "type": "boolean"
        },
        "midiSettingsAvailable": {
          "type": "boolean"
        },
        "modules": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "id": {
                "type": "integer",
                "minimum": 1,
                "maximum": 4294967295
              },
              "moduleId": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "title": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "description": {
                "type": "string"
              },
              "missing": {
                "type": "boolean"
              },
              "x": {
                "type": "number"
              },
              "y": {
                "type": "number"
              },
              "inputs": {
                "type": "array",
                "items": {
                  "type": "object",
                  "properties": {
                    "id": {
                      "type": "string",
                      "minLength": 1,
                      "maxLength": 256
                    },
                    "label": {
                      "type": "string"
                    },
                    "kind": {
                      "enum": [
                        "audio",
                        "control",
                        "event",
                        "modulation",
                        "gate",
                        "clock"
                      ]
                    }
                  },
                  "required": [
                    "id",
                    "label",
                    "kind"
                  ],
                  "additionalProperties": true
                }
              },
              "outputs": {
                "type": "array",
                "items": {
                  "type": "object",
                  "properties": {
                    "id": {
                      "type": "string",
                      "minLength": 1,
                      "maxLength": 256
                    },
                    "label": {
                      "type": "string"
                    },
                    "kind": {
                      "enum": [
                        "audio",
                        "control",
                        "event",
                        "modulation",
                        "gate",
                        "clock"
                      ]
                    }
                  },
                  "required": [
                    "id",
                    "label",
                    "kind"
                  ],
                  "additionalProperties": true
                }
              },
              "parameters": {
                "type": "array",
                "items": {
                  "type": "object",
                  "properties": {
                    "id": {
                      "type": "string",
                      "minLength": 1,
                      "maxLength": 256
                    },
                    "legacyIndex": {
                      "type": "integer",
                      "minimum": 0
                    },
                    "name": {
                      "type": "string",
                      "minLength": 1,
                      "maxLength": 256
                    },
                    "minimum": {
                      "type": "number"
                    },
                    "maximum": {
                      "type": "number"
                    },
                    "value": {
                      "type": "number"
                    },
                    "unit": {
                      "type": "string"
                    },
                    "display": {
                      "type": "string"
                    },
                    "taper": {
                      "type": "string"
                    }
                  },
                  "required": [
                    "id",
                    "name",
                    "minimum",
                    "maximum",
                    "value",
                    "display"
                  ],
                  "additionalProperties": true
                }
              }
            },
            "required": [
              "id",
              "moduleId",
              "title",
              "missing",
              "x",
              "y",
              "inputs",
              "outputs",
              "parameters"
            ],
            "additionalProperties": true
          }
        },
        "connections": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "connectionKey": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "kind": {
                "type": "string"
              },
              "from": {
                "type": "object",
                "properties": {
                  "module": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": 4294967295
                  },
                  "port": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "module",
                  "port"
                ],
                "additionalProperties": false
              },
              "to": {
                "type": "object",
                "properties": {
                  "module": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": 4294967295
                  },
                  "port": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "module",
                  "port"
                ],
                "additionalProperties": false
              },
              "channels": {
                "type": "integer",
                "minimum": 0
              },
              "amount": {
                "type": "number"
              },
              "feedback": {
                "type": "boolean"
              }
            },
            "required": [
              "connectionKey",
              "kind",
              "from",
              "to"
            ],
            "additionalProperties": true
          }
        },
        "palette": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "name": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "description": {
                "type": "string"
              }
            },
            "required": [
              "id",
              "name"
            ],
            "additionalProperties": true
          }
        },
        "running": {
          "type": "boolean"
        },
        "ready": {
          "type": "boolean"
        },
        "cleanupPending": {
          "type": "boolean"
        },
        "statusText": {
          "type": "string"
        },
        "errorText": {
          "type": "string"
        },
        "noticeText": {
          "type": "string"
        },
        "diagnosticText": {
          "type": "string"
        },
        "outputNode": {
          "type": "integer",
          "minimum": 0
        },
        "peak": {
          "type": "number",
          "minimum": 0
        },
        "cpuLoad": {
          "type": "number"
        },
        "underruns": {
          "type": "integer",
          "minimum": 0
        },
        "dirty": {
          "type": "boolean"
        },
        "patchName": {
          "type": "string"
        },
        "devices": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "name": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              },
              "legacyIndex": {
                "type": "integer",
                "minimum": -1
              }
            },
            "required": [
              "id",
              "name"
            ],
            "additionalProperties": true
          }
        },
        "selectedDevice": {
          "type": "integer",
          "minimum": -1
        },
        "sampleRate": {
          "type": "integer",
          "minimum": 1
        },
        "blockSize": {
          "type": "integer",
          "minimum": 1
        },
        "documentRevision": {
          "type": "string",
          "pattern": "^(0|[1-9][0-9]*)$",
          "maxLength": 20
        },
        "devicesEpoch": {
          "type": "string",
          "pattern": "^(0|[1-9][0-9]*)$",
          "maxLength": 20
        },
        "engineGeneration": {
          "type": "string",
          "pattern": "^(0|[1-9][0-9]*)$",
          "maxLength": 20
        },
        "capabilities": {
          "type": "object",
          "additionalProperties": {
            "type": [
              "boolean",
              "number",
              "string"
            ]
          }
        },
        "audioState": {
          "enum": [
            "uninitialized",
            "stopped",
            "running",
            "faulted",
            "cleanup_pending"
          ]
        }
      },
      "required": [
        "midiCounters",
        "midiEndpoints",
        "midiAuthorized",
        "midiSettingsAvailable",
        "modules",
        "connections",
        "palette",
        "running",
        "ready",
        "cleanupPending",
        "statusText",
        "errorText",
        "noticeText",
        "diagnosticText",
        "outputNode",
        "peak",
        "cpuLoad",
        "underruns",
        "dirty",
        "patchName",
        "devices",
        "selectedDevice",
        "sampleRate",
        "blockSize",
        "documentRevision",
        "devicesEpoch",
        "engineGeneration",
        "capabilities",
        "audioState"
      ],
      "additionalProperties": false
    },
    "commandArgs": {
      "session_hello": {
        "type": "object",
        "properties": {
          "minVersion": {
            "type": "integer",
            "minimum": 1
          },
          "maxVersion": {
            "type": "integer",
            "minimum": 1
          }
        },
        "required": [
          "minVersion",
          "maxVersion"
        ],
        "additionalProperties": false
      },
      "snapshot_get": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "catalog_get": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "file_pick": {
        "type": "object",
        "properties": {
          "purpose": {
            "enum": [
              "openPatch",
              "savePatch",
              "saveWav",
              "saveDiagnostics"
            ]
          }
        },
        "required": [
          "purpose"
        ],
        "additionalProperties": false
      },
      "patch_import": {
        "type": "object",
        "properties": {
          "jsonText": {
            "type": "string",
            "minLength": 1,
            "maxLength": 2097152
          }
        },
        "required": [
          "jsonText"
        ],
        "additionalProperties": false
      },
      "patch_export": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "telemetry_subscribe": {
        "type": "object",
        "properties": {
          "stream": {
            "enum": [
              "meters",
              "scope"
            ]
          },
          "hz": {
            "enum": [
              30,
              60
            ]
          },
          "target": {
            "type": "object",
            "properties": {
              "module": {
                "type": "integer",
                "minimum": 1,
                "maximum": 4294967295
              },
              "port": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              }
            },
            "required": [
              "module",
              "port"
            ],
            "additionalProperties": false
          }
        },
        "required": [
          "stream",
          "hz"
        ],
        "additionalProperties": false
      },
      "telemetry_unsubscribe": {
        "type": "object",
        "properties": {
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "subscriptionId"
        ],
        "additionalProperties": false
      },
      "midi_authorize": {
        "type": "object",
        "properties": {
          "enabled": {
            "type": "boolean"
          }
        },
        "required": [
          "enabled"
        ],
        "additionalProperties": false
      },
      "midi_endpoint_set": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "systemName": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "enabled": {
            "type": "boolean"
          }
        },
        "required": [
          "name",
          "systemName",
          "enabled"
        ],
        "additionalProperties": false
      },
      "midi_refresh": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "diagnostics_get": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "diagnostics_save": {
        "type": "object",
        "properties": {
          "fileHandle": {
            "type": "string",
            "minLength": 12,
            "maxLength": 256
          }
        },
        "required": [
          "fileHandle"
        ],
        "additionalProperties": false
      },
      "parameter_set": {
        "type": "object",
        "properties": {
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "parameterId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "value": {
            "type": "number"
          }
        },
        "required": [
          "nodeId",
          "parameterId",
          "value"
        ],
        "additionalProperties": false
      },
      "module_move": {
        "type": "object",
        "properties": {
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "x": {
            "type": "number"
          },
          "y": {
            "type": "number"
          }
        },
        "required": [
          "nodeId",
          "x",
          "y"
        ],
        "additionalProperties": false
      },
      "module_add": {
        "type": "object",
        "properties": {
          "moduleType": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "moduleType"
        ],
        "additionalProperties": false
      },
      "module_remove": {
        "type": "object",
        "properties": {
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          }
        },
        "required": [
          "nodeId"
        ],
        "additionalProperties": false
      },
      "connection_add": {
        "type": "object",
        "properties": {
          "kind": {
            "enum": [
              "signal",
              "modulation",
              "event"
            ]
          },
          "from": {
            "type": "object",
            "properties": {
              "module": {
                "type": "integer",
                "minimum": 1,
                "maximum": 4294967295
              },
              "port": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              }
            },
            "required": [
              "module",
              "port"
            ],
            "additionalProperties": false
          },
          "to": {
            "type": "object",
            "properties": {
              "module": {
                "type": "integer",
                "minimum": 1,
                "maximum": 4294967295
              },
              "port": {
                "type": "string",
                "minLength": 1,
                "maxLength": 256
              }
            },
            "required": [
              "module",
              "port"
            ],
            "additionalProperties": false
          },
          "amount": {
            "type": "number",
            "minimum": -1,
            "maximum": 1
          },
          "feedback": {
            "type": "boolean"
          },
          "stereoPair": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "kind",
          "from",
          "to"
        ],
        "additionalProperties": false,
        "allOf": [
          {
            "if": {
              "properties": {
                "kind": {
                  "const": "modulation"
                }
              },
              "required": [
                "kind"
              ]
            },
            "then": {
              "required": [
                "amount"
              ]
            }
          }
        ]
      },
      "connection_remove": {
        "type": "object",
        "properties": {
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "connectionKey"
        ],
        "additionalProperties": false
      },
      "audio_start": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "audio_stop": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "keyboard_gate": {
        "type": "object",
        "properties": {
          "on": {
            "type": "boolean"
          }
        },
        "required": [
          "on"
        ],
        "additionalProperties": false
      },
      "keyboard_note": {
        "type": "object",
        "properties": {
          "pitch": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "on": {
            "type": "boolean"
          }
        },
        "required": [
          "pitch",
          "on"
        ],
        "additionalProperties": false
      },
      "keyboard_panic": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "audio_device_select": {
        "type": "object",
        "properties": {
          "deviceId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          }
        },
        "required": [
          "deviceId",
          "devicesEpoch"
        ],
        "additionalProperties": false
      },
      "audio_devices_refresh": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      },
      "patch_output_select": {
        "type": "object",
        "properties": {
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "portId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "busChannel": {
            "type": "integer",
            "minimum": 0,
            "maximum": 15
          }
        },
        "required": [
          "nodeId"
        ],
        "additionalProperties": false
      },
      "patch_save": {
        "type": "object",
        "properties": {
          "fileHandle": {
            "type": "string",
            "minLength": 12,
            "maxLength": 256
          }
        },
        "required": [
          "fileHandle"
        ],
        "additionalProperties": false
      },
      "patch_load": {
        "type": "object",
        "properties": {
          "fileHandle": {
            "type": "string",
            "minLength": 12,
            "maxLength": 256
          }
        },
        "required": [
          "fileHandle"
        ],
        "additionalProperties": false
      },
      "render_demo": {
        "type": "object",
        "properties": {
          "fileHandle": {
            "type": "string",
            "minLength": 12,
            "maxLength": 256
          }
        },
        "required": [
          "fileHandle"
        ],
        "additionalProperties": false
      },
      "patch_reset": {
        "type": "object",
        "properties": {},
        "required": [],
        "additionalProperties": false
      }
    },
    "replyResults": {
      "session_hello": {
        "type": "object",
        "properties": {
          "negotiatedVersion": {
            "const": 1
          },
          "sessionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "capabilities": {
            "type": "object",
            "additionalProperties": true
          }
        },
        "required": [
          "negotiatedVersion",
          "sessionId",
          "capabilities"
        ],
        "additionalProperties": false
      },
      "snapshot_get": {
        "type": "object",
        "properties": {
          "snapshot": {
            "$ref": "#/$defs/snapshot"
          }
        },
        "required": [
          "snapshot"
        ],
        "additionalProperties": false
      },
      "catalog_get": {
        "type": "object",
        "properties": {
          "palette": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "id": {
                  "type": "string",
                  "minLength": 1,
                  "maxLength": 256
                },
                "name": {
                  "type": "string",
                  "minLength": 1,
                  "maxLength": 256
                },
                "description": {
                  "type": "string"
                }
              },
              "required": [
                "id",
                "name"
              ],
              "additionalProperties": true
            }
          }
        },
        "required": [
          "palette"
        ],
        "additionalProperties": false
      },
      "file_pick": {
        "type": "object",
        "properties": {
          "fileHandle": {
            "type": "string",
            "minLength": 12,
            "maxLength": 256
          },
          "displayName": {
            "type": "string"
          }
        },
        "required": [
          "fileHandle",
          "displayName"
        ],
        "additionalProperties": false
      },
      "patch_import": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "patch_export": {
        "type": "object",
        "properties": {
          "jsonText": {
            "type": "string"
          },
          "suggestedName": {
            "type": "string"
          }
        },
        "required": [
          "jsonText",
          "suggestedName"
        ],
        "additionalProperties": false
      },
      "telemetry_subscribe": {
        "type": "object",
        "properties": {
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "subscriptionId"
        ],
        "additionalProperties": false
      },
      "telemetry_unsubscribe": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "midi_authorize": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "midi_endpoint_set": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "midi_refresh": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "diagnostics_get": {
        "type": "object",
        "properties": {
          "diagnostics": {
            "type": "object",
            "additionalProperties": true
          }
        },
        "required": [
          "diagnostics"
        ],
        "additionalProperties": false
      },
      "diagnostics_save": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "parameter_set": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "module_move": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "module_add": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "module_remove": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "connection_add": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "connection_remove": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "audio_start": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "audio_stop": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "keyboard_gate": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "keyboard_note": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "keyboard_panic": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "audio_device_select": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "audio_devices_refresh": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "patch_output_select": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "patch_save": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "patch_load": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "render_demo": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      },
      "patch_reset": {
        "type": "object",
        "properties": {
          "accepted": {
            "const": true
          },
          "revision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "connectionKey": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "devicesEpoch": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "status": {
            "type": "string"
          },
          "display": {
            "type": "string"
          },
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          }
        },
        "required": [
          "accepted"
        ],
        "additionalProperties": false
      }
    },
    "eventData": {
      "document_changed": {
        "type": "object",
        "properties": {
          "documentRevision": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          },
          "snapshot": {
            "$ref": "#/$defs/snapshot"
          },
          "reason": {
            "type": "string"
          }
        },
        "required": [
          "documentRevision"
        ],
        "additionalProperties": true
      },
      "parameter_changed": {
        "type": "object",
        "properties": {
          "nodeId": {
            "type": "integer",
            "minimum": 1,
            "maximum": 4294967295
          },
          "parameterId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "value": {
            "type": "number"
          },
          "display": {
            "type": "string"
          }
        },
        "required": [
          "nodeId",
          "parameterId",
          "value",
          "display"
        ],
        "additionalProperties": true
      },
      "status_changed": {
        "type": "object",
        "properties": {
          "running": {
            "type": "boolean"
          },
          "ready": {
            "type": "boolean"
          },
          "cleanupPending": {
            "type": "boolean"
          },
          "errorText": {
            "type": "string"
          },
          "noticeText": {
            "type": "string"
          },
          "dirty": {
            "type": "boolean"
          }
        },
        "required": [],
        "additionalProperties": true
      },
      "meters_changed": {
        "type": "object",
        "properties": {
          "peak": {
            "type": "number"
          },
          "cpuLoad": {
            "type": "number"
          },
          "underruns": {
            "type": "integer",
            "minimum": 0
          },
          "diagnosticText": {
            "type": "string"
          },
          "midiCounters": {
            "type": "object",
            "additionalProperties": true
          },
          "telemetryDropped": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "peak",
          "cpuLoad",
          "underruns"
        ],
        "additionalProperties": true
      },
      "scope_chunk": {
        "type": "object",
        "properties": {
          "subscriptionId": {
            "type": "string",
            "minLength": 1,
            "maxLength": 256
          },
          "channels": {
            "type": "integer",
            "minimum": 1,
            "maximum": 16
          },
          "min": {
            "type": "array",
            "items": {
              "type": "number"
            },
            "maxItems": 4096
          },
          "max": {
            "type": "array",
            "items": {
              "type": "number"
            },
            "maxItems": 4096
          },
          "frame": {
            "type": "string",
            "pattern": "^(0|[1-9][0-9]*)$",
            "maxLength": 20
          }
        },
        "required": [
          "subscriptionId",
          "channels",
          "min",
          "max",
          "frame"
        ],
        "additionalProperties": false
      },
      "session_changed": {
        "type": "object",
        "properties": {
          "reason": {
            "type": "string"
          }
        },
        "required": [
          "reason"
        ],
        "additionalProperties": true
      },
      "capabilities_changed": {
        "type": "object",
        "properties": {
          "capabilities": {
            "type": "object",
            "additionalProperties": true
          }
        },
        "required": [
          "capabilities"
        ],
        "additionalProperties": false
      }
    }
  }
}
```


## 8. Offline host test plan (not run in this research pass)

**Test setup:** no real PortAudio, MIDI, ALSA, WASAPI or LAN endpoints. Use injected PortAudio function table from native/ui/audio_device.h:14-33 as the behavioral model, a fake MIDI Backend as accepted by native/ui/studio_controller.h:56-59, fake disk picker/store and deterministic fake clock. Loopback tests may bind localhost only, per AGENTS.md. Add all test cases to default Windows/Linux and Pi headless CI where applicable; no user machine or Pi device access.

| Case | Setup / assertions |
|---|---|
| H01 wire-schema corpus | every mapped command has valid request, reply and event examples; invalid/unknown op, version, missing args, wrong types, extra fields rejected; include emoji/UTF8, escaped keys, duplicate raw keys |
| H02 auth/locality | fake token valid/invalid, strict WS Origin, loopback binding, WS handshake flood, Tauri allowlist; no unauthorized filesystem/module/device access, no LAN listener |
| H03 request id and conflict | duplicate request ID same payload idempotent, same ID different payload rejected; concurrent expectedRevision collision; bad devicesEpoch rejected |
| H04 snapshot parity | full 24 Q_PROPERTY-equivalent fields and types, stable module/port IDs, missing module projection, resolved channel counts; Qt golden fixture parity from native/ui/studio_controller.cpp:138-205 |
| H05 notifications | documentChanged/parameterChanged/statusChanged/metersChanged corresponding event topics, correct seq/revision ordering; reconnect recovers missing events with full snapshot |
| H06 module add/remove | 64-node cap, positive stable IDs, no silent reuse of existing IDs, remove attached connections, failed prepare leaves old engine/document/revision |
| H07 connect/disconnect | signal vs event vs modulation amount, feedback, multi-event-in, single signal input, cycle preflight, stale connectionKey, stale revision, invalid endpoints |
| H08 parameter semantics | non-finite/NaN and extremes, stable string lookup, taper/plain-unit conversion, value display, 0..100 sketch mapping isolated in UI, rapid knob coalescing and final-release flush |
| H09 save/load losslessness | native v1 migration, untouched original bytes + stem.v1.json backup, native v2 unknown root/module/cable/parameter/state/outputs/device fields, full unknown module, JSON duplicates, UTF8 keys, output channel 0+1 |
| H10 disk failure | over 2MiB, open/read/write/flush/fsync/rename/backup collisions, existing invalid target, file cancel, path traversal and forged handle; no overwritten originals, no false dirty-clear |
| H11 audio state | never opens real device; fake init/start/stop/abort/close failures, partial failure, cleanup_pending, retry stop, no early engine free, no accidental audio autostart |
| H12 realtime invariants | allocation-watch around every callback with DSP+MIDI events; bounded callback time under fake backend, no locks/I/O/JSON/log; checked under TSAN/ASAN/UBSAN as possible |
| H13 queue pressure | parameter flood, critical gate/note-off reserve, queue full rejection, emergency panic/stop; control enqueue accepted vs applied generation ack differentiated |
| H14 MIDI consent | offline/test mode never enumerates real endpoints; fake authorization persistence failure/rollback, endpoint rename/hotplug, duplicate names with different system_name, counters |
| H15 devices | fake 0/1/many outputs, default -1, device epoch changes on refresh, selected-device stale index, stereo format not supported, CPU/xrun overflow counters |
| H16 telemetry/backpressure | 30Hz and 60Hz fake-clock ticks, latest-only meter/scope, constrained 64KiB buffer, slow/no reader, no telemetry causing control starvation, seq-gap recovery |
| H17 scope capability | cap false disables subscribing, UI says illustrative; cap true uses fake SPSC samples, bounds 128 bin pairs/ch, no callback serialization; verify dropped frames counted |
| H18 keyboard/focus | A–K/space key down/up, blur/lost controlling session, stuck key recovery, on/off ordering and emergency note-off; existing QML precedent native/ui/qml/Main.qml:32-50,160-164,210-228 |
| H19 demo render | fixed 192000 frames, 4s mono/48k/PCM16, 2s gate, exact 384044 bytes, no overwrite, zero live-device starts, deterministic hash |
| H20 UI contract | DOM-level interactions on the **owner-approved sketch**: drag, cable hit test, scaled cursor, pan/zoom, keyboard access, undo/discard/save-failure, focus; compare visual screenshots at 1400x900 and minimum 1000x650 vs approved reference, not arbitrary prototype-derived pixels |
| H21 platform packaging | Windows host + WebView2/Tauri sidecar and Raspberry Pi 5 ARM64 package with -DWIRESCAPE_BUILD_UI=OFF; scan DLL/SO dependencies to prove no Qt, measure startup, memory and steady-state 30/60Hz CPU; results must be reported rather than guessed |
| H22 malicious protocol input | fuzz JSON depth/length, duplicate keys, huge arrays, decimal-string overflow, invalid file handles/ports, Unicode normalization, request id cache exhaustion, malformed scope bins |
| H23 migration matrix | fixtures for native v1/v2, legacy visual.study v4 **rejected from runtime**, v99 future preserved, unknown modules, empty outputs, stereo outputs, legacy parameter mapping and custom extension fields |
| H24 recovery and shutdown | repeated frontend crashes, missing heartbeat, render failure, backend close timeouts; no engine use-after-free and no false ready/playback |
| H25 generation and stale consumers | late parameter command after module removal, timestamped MIDI alongside patch swap, stale meter callback, snapshot revision race, monotonic engine generation/applied ack |

**Required acceptance evidence before implementation merge:** (a) schema checked with an actual Draft-2020-12 validator plus golden message corpus, (b) all offline fake-device tests passing on Windows and ARM64, (c) lossless patch round-trip and v1 backup corpus, (d) callback allocation/race test and explicit no-device audit, (e) owner-reviewed HTML interaction/screenshots, (f) measured Pi 5 telemetry CPU/memory and scheduling results. This research cannot claim any of these are complete.

## 9. Recommended work breakdown (post-decision, not authorization)

1. Extract Qt-free document service reusing canonical reader semantics and tests; keep ABI headers unchanged. Build real host CI with audio off.
2. Specify stable descriptor catalog and canonical v2 projection; implement serial control thread, revisioned snapshots and request/reply/error envelopes. Test all 23 command mappings against fake engine/audio/MIDI.
3. Attach browser or Tauri front end via one transport adapter, preserve sketch markup/visual tokens, replace browser-only save with canonical patch; implement multi-output wiring and missing-module recovery before treating as a functional editor.
4. Add telemetry, backpressure, crash/focus note-off, safe file handles and packaging. Gate real scope separately from illustrative animation.
5. Perform paired owner UI visual and behavior acceptance on Windows, then Pi 5 performance acceptance using offline runner. No real-device session until explicit owner action.

**Research closeout:** This document deliberately makes no patch format, ABI, module ID, realtime or merge-gate change. No existing file was edited. No tests or hardware access were performed.

## Decisions needed from the planner

1. **Packaged transport:** (A) Tauri IPC for production + tokenized loopback WS for web/dev; (B) localhost WebSocket for every build; (C) IPC only. **Recommend A** for a browser-development escape hatch without committing the packaged product to an open listener.
2. **QtCore document removal:** (A) port document/editor/atomic writer to Qt-free C++ retaining the existing regression suite; (B) ship QtCore hidden in host; (C) temporarily accept runtime-only v2 JSON without migration/atomic save. **Recommend A**; B may be a development bridge, never counted as Qt-free.
3. **HTML module identity:** (A) visually map supported controls/ports to existing native descriptors and label unsupported CHAOS/modulator/IO features; (B) build the three sketch modules first; (C) keep mock data as if live. **Recommend A**, plan B later with explicit DSP tasks; reject C.
4. **Cable behavior:** (A) ABI2 signal/event/modulation + feedback with capability/descriptor validation and informative color hints; (B) clone Qt's audio/control/event strict gate and DAG-only behavior. **Recommend A**; Qt rules would silently restrict patch v2.
5. **Patch/UI persistence:** (A) canonical patch v2 with additive namespaced visual metadata, stable uint32 nodes and string ports/params; (B) visual.study v4 as executable patch; (C) external sidecar UI state and native patch separate. **Recommend A** with explicit study import/mapping, not automatic cast.
6. **Graph changes during playback:** (A) edits staged/committed only when quiescent initially; (B) immediate hot swap with new parallel graph/published generation; (C) mutate live engine directly. **Recommend A** now; B requires separate realtime proof and scoped tests; reject C.
7. **Scope and meters:** (A) real peak/cpu/xrun now, opt-in instrument scope only when native tap exists; (B) ship illustrative animation with explicit label; (C) ship fake meters labelled live. **Recommend A**, allow B for visually nonauthoritative waveform placeholder, reject C.
8. **Host filesystem access:** (A) native picker-issued short-lived opaque file handles plus bounded JSON import/export; (B) arbitrary absolute paths received over socket. **Recommend A** for privilege separation and offline tests.
9. **UI acceptance gate:** (A) owner-approved sketch screenshot/interaction contract plus fake-device scenario matrix on both platforms; (B) functional API tests alone; (C) visual review alone. **Recommend A**; both visuals and runtime semantics are part of the release requirement.

