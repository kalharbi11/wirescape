<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Research: Qt UI surface and Qt-free Studio host protocol

**Status:** owner-authorized research proposal, not a deployed contract. **Basis:** `native/ui/studio_controller.h`, `native/ui/studio_controller.cpp`, the current QML files, native document/engine/control sources and `docs/specs/patch_v2_draft.schema.json`. **Scope:** default branch on 2026-11-01; no source edits, device access, builds or CI evidence. The optional single-file “WIRESCAPE Studio 01” owner sketch was not attached to this request; it is not treated as the source of current engine behavior.

## Executive findings

**Observed facts.** One context object named `studio` points to `StudioController` and a second value `studioFontFamily` is presentation-only (`native/ui/main.cpp:57-62`). The controller declares **24 Q_PROPERTY values, 23 Q_INVOKABLE methods, and 4 signals** (`native/ui/studio_controller.h:19-42`, `native/ui/studio_controller.h:65-114`). There are **three QML files containing Studio bridge calls/reads**: `native/ui/qml/Main.qml`, `ModuleCard.qml` and `ParameterControl.qml` (citations in the inventory). QML pan/zoom, cable geometry, mouse gestures, focus handling, visual styles, dialogs and key-repeat suppression are presentation code, not engine calls (`native/ui/qml/Main.qml:97-163`, `native/ui/qml/Main.qml:206-228`, `native/ui/qml/Main.qml:332-415`). The 50 ms controller timer makes the current meter notification nominally **20 Hz**, not 30–60 Hz (`native/ui/studio_controller.cpp:92-103`).

The headless engine and control/MIDI services **already compile independently of Qt**; the Qt-backed editable patch inspection/migration/save library remains separate from the Qt-free runtime loader (`native/engine2/CMakeLists.txt:141-162`, `native/document/CMakeLists.txt:8-10`, `native/document/CMakeLists.txt:25-32`, `native/document/runtime_patch.h:5-10`). A Qt-free host must not silently substitute the runtime loader for a preservation-capable editor. The current v1-shaped editor view is an adapter over persisted **native v2**, and a lossy replacement would violate ID and unknown-field requirements (`native/document/app_patch.cpp:16-48`, `native/document/patch_document.cpp:145-218`).

**Recommendation.** Build a control-thread-owned headless host exposing the **semantic operations below**; use a Tauri IPC bridge for packaged Windows, with an explicitly secured loopback WebSocket adapter as an option for the Pi/browser. Keep one authoritative in-memory native-v2 document, emit revisioned JSON snapshots/deltas, never send WebSocket/JSON/UI work to the audio callback, and leave graph preparation/destruction quiescent. Protocol **v1** (in this report) is unrelated to patch **v2** and ABI **2**.

## 1. Complete QML-facing inventory

**Thread key:** **UI** means the Qt GUI/event-loop thread that constructs and owns `StudioController` (`native/ui/main.cpp:18-20`, `native/ui/main.cpp:57-62`). QML property reads, invokables and all four emissions execute on this owner thread in the current call paths; `QTimer` is attached there (`native/ui/studio_controller.cpp:52-56`, `native/ui/studio_controller.cpp:92-103`). **RT** is the PortAudio callback thread: only the audio adapter's callback and `app_engine_render_live` render there, not the QObject (`native/ui/audio_device.cpp:173-200`, `native/ui/studio_controller.cpp:381-399`). MIDI backend callbacks are separate producer contexts (`native/control/midi_host.cpp:42-69`). File I/O and device enumeration in this controller currently run synchronously on UI.

### A. 24 readable properties

The signature/source for each row is `native/ui/studio_controller.h:19-42` and its getter in `native/ui/studio_controller.h:61-89`; side effects below refer to *getting* the property (not to the operations that change it). “—” means **no current QML use found**, but preserve it for API parity.

| Property / return | QML reads (file:line) | Thread | Getter effect / provenance |
|---|---|---|---|
| `midiCounters: QVariantMap` | `Main.qml:168-177` | UI | Parses cached/current MIDI diagnostics JSON; service counters may be read (`studio_controller.cpp:627`, `midi_host.cpp:108-124`). |
| `midiEndpoints: QVariantList` | `Main.qml:179-185` | UI | **May enumerate** authorized MIDI devices when stopped; reads enabled selection while playing (`studio_controller.cpp:579-590`). |
| `midiAuthorized: bool` | `Main.qml:175,183` | UI | Reads MIDI permission flag (`studio_controller.h:63`). |
| `midiSettingsAvailable: bool` | `Main.qml:175-176` | UI | Reads whether QSettings preferences are active (`studio_controller.h:64`). |
| `modules: QVariantList` | `Main.qml:15,404-407`; data in `ModuleCard.qml:7-20,58-79,122-138` | UI | Allocates presentation descriptors from document/catalog; missing modules keep saved endpoint labels (`studio_controller.cpp:145-175`). |
| `connections: QVariantList` | `Main.qml:16,360-398,423` | UI | Builds presentation cables; adds calculated `channels` widths, not a raw patch-v2 graph (`studio_controller.cpp:177-205`). |
| `palette: QVariantList` | `Main.qml:256-279` | UI | Enumerates loaded descriptor catalogue (`studio_controller.cpp:138-143`). |
| `running: bool` | `Main.qml:49,104,115,175,183,187,262,305,309-314,423-425,445,449,471`; `ModuleCard.qml:34,145` | UI | Reads audio state (`studio_controller.h:71`). |
| `ready: bool` | `Main.qml:314` | UI | Reads engine readiness (`studio_controller.h:72`). |
| `cleanupPending: bool` | `Main.qml:314` | UI | Tests stream still owned and not running (`studio_controller.h:73`). |
| `statusText: QString` | `Main.qml:472` | UI | Formats running/ready/incomplete summary (`studio_controller.cpp:206`). |
| `errorText: QString` | `Main.qml:94,472` | UI | Reads last error (`studio_controller.h:75`). |
| `noticeText: QString` | `Main.qml:472` | UI | Reads last notice (`studio_controller.h:76`). |
| `diagnosticText: QString` | `Main.qml:473` | UI | Reads engine meter + callback faults and formats diagnostic summary (`studio_controller.cpp:468-472`). |
| `outputNode: int` | `ModuleCard.qml:144-145` | UI | Reads v1 editor view's selected node (`studio_controller.h:80`). |
| `peak: double` | `Main.qml:462` | UI | Reads 50-ms cached meter peak (`studio_controller.cpp:99-100`). |
| `cpuLoad: double` | `Main.qml:473` | UI | Atomic load, scaled to percent (`studio_controller.h:82`, `audio_device.cpp:99-101`). |
| `underruns: int` | `Main.qml:473` | UI | Atomic xrun count (`studio_controller.h:83`, `audio_device.cpp:99-101`). |
| `dirty: bool` | `Main.qml:12,60,76,304` | UI | Reads unsaved-edit state (`studio_controller.h:84`). |
| `patchName: QString` | `Main.qml:12,304` | UI | Reads display name (`studio_controller.h:85`). |
| `devices: QVariantList` | `Main.qml:449-452` | UI | Enumerates PortAudio devices if backend initialized, adds “system default” (`audio_device.cpp:85-97`). |
| `selectedDevice: int` | `Main.qml:452` | UI | Reads selected device ID (`studio_controller.h:87`). |
| `sampleRate: int` | `Main.qml:473` | UI | Constant **48000** (`studio_controller.h:88`). |
| `blockSize: int` | `Main.qml:473` | UI | Constant **256** (`studio_controller.h:89`). |

`modules` descriptor payload: `id,moduleId,title,description,x,y,missing,inputs[],outputs[],parameters[]`; each port has `id,label,kind` and each parameter `id,name,minimum,maximum,value,unit,display` (`native/ui/studio_controller.cpp:145-174`). `connections` additionally carries presentation `channels` (`native/ui/studio_controller.cpp:199-203`). `palette` entries are `id,name,description` (`native/ui/studio_controller.cpp:138-142`). Device entries are `id,name` (`native/ui/audio_device.cpp:85-95`); MIDI endpoint UI entries are `name,systemName,enabled` (`native/ui/studio_controller.cpp:579-590`).

### B. All 23 Q_INVOKABLE methods

The declaration/return types are `native/ui/studio_controller.h:65-107`. Every call below is made on **UI**, and all observable effects happen there or submit bounded events to the render ingress, except that PortAudio/MIDI start their own callbacks. “—” means no present QML call (not deletion permission).

| Method (arguments) → return | QML caller (file:line) | Thread | Side effects / current constraints |
|---|---|---|---|
| `setMidiAuthorized(bool) → void` | `Main.qml:175` | UI | Persist QSettings permission; potentially instantiate real backend; stopped-only (`studio_controller.cpp:591-615`). |
| `setMidiEndpoint(QString name, QString system, bool enabled) → void` | `Main.qml:184` | UI | Update enabled MIDI devices and dirty flag; authorization and stop required (`studio_controller.cpp:618-625`). |
| `refreshMidi() → void` | `Main.qml:187` | UI | Notify status only when quiescent; endpoint getter then enumerates (`studio_controller.cpp:617`, `studio_controller.cpp:579-590`). |
| `diagnostics() → QVariantMap` | — | UI | Read audio, engine and session counters; adds timestamp and graph info (`studio_controller.cpp:452-467`). |
| `saveDiagnostics(QUrl) → bool` | — | UI | Atomic JSON diagnostics write; notice/error (`studio_controller.cpp:473-478`). |
| `setParameter(int node,int parameter,double value) → bool` | `ParameterControl.qml:52,194` | UI → ingress if live | Validate finite/range, enqueue live parameter, update editor values, set dirty, emit parameter/status (`studio_controller.cpp:310-317`). |
| `moveModule(int node,double x,double y) → void` | `ModuleCard.qml:28` | UI | Change document coordinates, dirty/status; **no** graph recompile (`studio_controller.cpp:319-323`). |
| `addModule(QString id) → void` | `Main.qml:278` | UI | Allocate smallest free positive node ID, seed parameters, commit topology stopped-only (`studio_controller.cpp:325-333`). |
| `removeModule(int node) → void` | `ModuleCard.qml:35` | UI | Remove node/incident cables, clear selected output if needed, commit stopped-only (`studio_controller.cpp:335-340`). |
| `connectPorts(int from,QString out,int to,QString in) → void` | `Main.qml:111` | UI | Validate port kinds, append cable, compile staged graph stopped-only (`studio_controller.cpp:342-358`). |
| `disconnectCable(int index) → void` | `Main.qml:425` | UI | Delete cable by **array index**, commit stopped-only (`studio_controller.cpp:360`). |
| `startAudio() → void` | `Main.qml:314` | UI + starts RT/MIDI | Compile, initialize/open PortAudio stereo, then start MIDI; errors reported (`studio_controller.cpp:381-399`). |
| `stopAudio() → void` | `Main.qml:314` | UI + joins callbacks | Stop MIDI/PortAudio; if safe, reset and cache last meter; cleanup may remain pending (`studio_controller.cpp:401`, `audio_device.cpp:145-160`). |
| `setGate(bool on) → void` | `Main.qml:35,164,218,226,436` | UI → ingress | Live-only gate fan-out to document nodes (`studio_controller.cpp:402`). |
| `note(int pitch,bool on) → void` | `Main.qml:36,218,226` | UI → ingress | Pitch 0–127; first host.keyboard receives notes, else live fan-out; stable note ID pitch+2 (`studio_controller.cpp:403-415`). |
| `panic() → void` | `Main.qml:164,437` | UI → engine panic/reset | Request panic if callback owns engine, otherwise reset; emit status (`studio_controller.cpp:416-425`). |
| `setDevice(int id) → void` | `Main.qml:456` | UI | Select device only while stopped (`studio_controller.cpp:379`). |
| `refreshDevices() → void` | `Main.qml:445` | UI | Enumerates/reinitializes audio backend; rejects pending cleanup (`studio_controller.cpp:378`, `audio_device.cpp:66-77`). |
| `selectOutput(int node) → void` | `ModuleCard.qml:146` | UI | Change outputNode and stage topology while stopped (`studio_controller.cpp:428`). |
| `savePatch(QUrl) → bool` | `Main.qml:65` | UI | Convert editor to native v2, merge MIDI selections, atomic save/backup, update source and dirty (`studio_controller.cpp:430-450`). |
| `loadPatch(QUrl) → bool` | `Main.qml:199` | UI | Bounded read/classify, validate native v1/v2, stage engine and MIDI; reject while audio owns engine (`studio_controller.cpp:480-520`). |
| `renderDemo(QUrl) → void` | `Main.qml:204` | UI (blocking offline) | New-only WAV, **4 s mono / 48 kHz / 16-bit**, 2-second gates, no overwrite; notice (`studio_controller.cpp:523-539`). |
| `resetPatch() → void` | `Main.qml:71` | UI | Stop MIDI/audio, restore default **5 nodes / 5 cables**, recompile and clear dirty (`studio_controller.cpp:23-42`, `studio_controller.cpp:426`). |

**Non-QML public helpers:** `smokeCheck(QJsonObject&)` and `keyboardChordCheck(QJsonObject&)` are C++ diagnostics, **not Q_INVOKABLE**, used for offline tests (`native/ui/studio_controller.h:108-109`, `native/ui/studio_controller.cpp:541-577`). `midiMode(...)` determines whether interactive MIDI is allowed; not exposed to QML (`native/ui/studio_controller.h:44-54`). `studioFontFamily` is a second **QString context value**, not an engine capability (`native/ui/main.cpp:59-60`).

### C. Four QML-facing signals

| Signal (arguments) | QML handler / binding dependency (file:line) | Emitter and thread | Proposed event |
|---|---|---|---|
| `documentChanged()` | Data bindings `Main.qml:15-16,360,404` | UI after successful topology/load/reset (`studio_controller.cpp:306-307`, `studio_controller.cpp:426`, `studio_controller.cpp:520`) | `document.changed` + revision/snapshot |
| `parameterChanged(int node,int parameter,double value,QString display)` | `ParameterControl.qml:196-208` | UI after accepted parameter (`studio_controller.cpp:316`) | `parameter.changed` |
| `statusChanged()` | `Main.qml:47-50`; property bindings throughout `Main.qml:12-314,443-473` | UI after state/dirty/error transitions (`studio_controller.cpp:207`, `studio_controller.cpp:379`, `studio_controller.cpp:615`) | `status.changed` |
| `metersChanged()` | Reactive property bindings `Main.qml:168-177,462-473` | UI 50 ms timer and stop/recompile/load (`studio_controller.cpp:92-103`, `studio_controller.cpp:295`, `studio_controller.cpp:401`, `studio_controller.cpp:520`) | `meters.changed` |

**Presentation-only boundary:** front-end-specific dialogs (`Main.qml:190-205`), geometry/pan/zoom (`Main.qml:127-159,332-415`), control types and normalization (`ModuleCard.qml:92-139`, `ParameterControl.qml:39-56`), and keyboard focus-loss handling (`Main.qml:32-39,164,210-228`) remain web-side. The host owns acceptance, value/ID validation, lifecycle, persistence and events.

## 2. Qt dependencies outside native/ui

**Method:** narrow default-branch source search for Qt includes, symbols and `Qt6::Core` CMake linkage. Below is the **complete positive Qt-specific source/build set found outside native/ui**, divided into production path vs test tools; searches found **no Qt imports in `native/control/` or `native/io/`** (their app/MIDI and audio service implementations are standard C++: `native/control/app_engine.cpp:1-11`, `native/control/midi_host.cpp:1-8`, `native/io/audio/portaudio_service.cpp:1-17`). An include on its own is not evidence of linkage into an engine callback.

| File(s), Qt usage (file:line) | Blocks a Qt-free host? | Estimate to remove / replace (engineering days) |
|---|---|---|
| `native/document/document_reader.h:2-7,15-35`; `native/document/document_reader.cpp:2-6,34-101` — `QByteArray,QJsonObject,QJsonValue,QString,QVector,QSet` document detection / legacy inspection | **Yes, for equivalent editor/inspector API**; **no** for already-working Qt-free runtime execution | **1–2 d**: port classifier to nlohmann; retain original bytes/undefined-vs-null and invalid-format diagnostics; write fixtures. |
| `native/document/patch_document.h:2-5,8-28`; `native/document/patch_document.cpp:3-11,29-57,68-83,86-268` — Qt containers/JSON, migration, duplicate-key detection, `QSaveFile,QTemporaryFile,QFile` | **Yes for lossless editor v1→v2 and save**, not for headless play | **3–5 d**: port validation/catalog/migration, portable atomic replace and race-safe `.v1.json` backup; test byte-equivalent preservation and crashes. |
| `native/document/app_patch.h:2-10`; `native/document/app_patch.cpp:4-6,9-55` — Qt JSON editor view and engine build adapter | **Yes if host reuses current editor view**; no if it uses native-v2 model plus Qt-free runtime loader | **2–4 d**: native-v2 editing projection, metadata catalog mapping, stable IDs, rebuilt snapshots and staging. |
| `native/document/CMakeLists.txt:25-32` — optional `Qt6::Core` for `wirescape_document` | **Not for engine2/CLI**, but prevents the existing editor library from being a Qt-free dependency | **0.5–1 d** build target split after implementation above; maintain optional Qt tests during transition. |
| `native/engine2/tests/soak.cpp:19-22,91-103`; `native/engine2/CMakeLists.txt:172-184` — `WS_SOAK_DOCUMENT` switches on Qt `QFile` and `app_patch`, with conditional Qt linkage | **No production block**; blocks this optional test path on Qt-free builds | **0.5–1 d** migrate file/loader to existing `load_runtime_patch` and preserve equivalent test coverage. |
| `native/tests/document_reader_tests.cpp:2-4,15-32`; `native/tests/patch_document_tests.cpp:3-8,15-34`; `native/tests/patch_render_tests.cpp:5-12,22-30` | **No** (Qt-based test executables) | **1–2 d** parallel Qt-free fixtures/test adapters, then retire duplicate Qt tests only with planner permission. |
| `native/document/app_patch_tests.cpp:5-11,19-35`; `native/tests/golden_render.cpp:4-10,22-39,113-129` | **No** (test executables) | **1–2 d** rewire to native-v2 and std SHA/temp libraries; keep golden equality expectations. |

**Total proposed migration estimate:** **9–17.5 engineering days** for an equivalent Qt-free editor, optional soak and offline test migration (sum of the rows; uncertainty excludes package integration and cross-platform CI time). **No Qt removal is required from `native/control` or `native/io` in the observed sources**. The Qt-backed tests are conditionally built (`native/document/CMakeLists.txt:25-65`), and the base build guards `native/ui` behind `WIRESCAPE_BUILD_UI` (`native/CMakeLists.txt:17,99-101`). Existing Qt `AudioDevice` is entirely inside `native/ui`, while `native/io/audio/portaudio_service.*` is already a standard-C++ PortAudio host adapter (`native/ui/audio_device.h:4-6,34-50`, `native/io/audio/portaudio_service.cpp:18-68`).

**Important scope boundary:** `native/io/audio/portaudio_service.cpp:3` currently imports a PortAudio **header located at** `native/ui/third_party/portaudio.h`, but **does not link Qt**; moving/replacing that include path later is packaging hygiene, not a Qt-free blocker. Do not read or modify the third-party source under this research exception.

## 3. Native patch v2: required preservation semantics

**Facts from existing format and code:**

- Canonical persisted format `wirescape.native.patch` **version 2** requires `settings,modules,connections,outputs`; example carries sample rate **48000**, max block **256**, max channels **16**, feedback block **32**, seeds, stable string parameter IDs, modulation amount and stereo output entries (`docs/specs/patch_v2_draft.schema.json:1-19,23-108`; `docs/specs/patch_v2_draft.example.json:1-49`).
- Native-v2 module IDs are unsigned **1…2^32−1**, module type is `module`, stable parameter names live under `parameters`; edges use `kind=signal|modulation|event` and `from/to: {module,port}`; modulation requires `amount∈[-1,1]`; outputs use `bus_channel 0…15` (`docs/specs/patch_v2_draft.schema.json:23-104`). Keep both the **module-instance numeric ID** and the **descriptor's immutable string module/port/parameter IDs**.
- `read_patch` maps native v1 to v2 and retains the **entire original envelope under `preserved`**, plus each original module under module `preserved` (`native/document/patch_document.cpp:145-203`). Unknown v2 modules are marked inactive with preserved data (`native/document/patch_document.cpp:204-218`). The editor maps stable string IDs back into a v1-shaped visual model and back again (`native/document/app_patch.cpp:16-48`).
- Saving requires successful native-v2 validation, rejects malformed/future targets, writes a non-overwriting `.v1.json` sibling when upgrading v1, and commits via same-directory atomic `QSaveFile` (`native/document/patch_document.cpp:220-268`). Original v1 bytes are explicitly retained for backup (`native/document/patch_document.h:16-28`). The shared inspector caps files at **2 MiB** (`native/document/document_reader.h:8-9`).
- Qt-free `load_runtime_patch` caps **2 MiB**, depth **64**, **64 modules**, **128 cables**, and rejects duplicate JSON keys; it compiles and prepares graphs quiescently, **does not save** (`native/document/runtime_patch.cpp:28-39,47-76`; `native/document/DESIGN.md:4-14`). It rejects inactive/unavailable modules rather than serving as an editable recovery representation (`native/document/runtime_patch.cpp:49-54`). Do not mistake these runtime limits for all limits in the v2 draft schema.
- Current Studio controller's QML-facing adapter validates a **v1-shaped document only**, blocks cycles and one cable/input in that projection (`native/ui/studio_controller.cpp:212-262`); v2 format can express typed event/modulation/feedback features not representable through those QML actions (`docs/specs/patch_v2_draft.schema.json:55-90`). This is a **migration gap**, not authority to discard unsupported data.
- Engine2 supports delayed feedback and reserved event producers as a contract, but current UI operation semantics do not expose the full contract (`docs/specs/ABI2_PLAN.md:162-175,195-204`; `native/ui/studio_controller.cpp:342-358`).

**Recommended host document model.** Hold a parsed canonical v2 object **plus the exact original bytes** for provenance and v1 backup. Apply edit operations to a *copy*, preserving unknown document/module/cable/endpoint fields and original relative ordering; validate draft schema + native semantic invariants; derive UI-only `modules`/`connections` projections; build/stage replacement engine only when stopped and fully quiescent; atomically publish the new document and increment `revision` on success. Do **not** round-trip v2 through the v1 editor schema in the new host. Represent unresolved processors as visible, editable, **non-executable** nodes; do not reject the document merely because a module is missing. Runtime preparation remains strict. Update `devices` by identity (`name` + `system_name`) without overwriting unrelated service records.

**Cable identity:** the v2 schema has no required persistent cable ID (`docs/specs/patch_v2_draft.schema.json:55-91`) and current UI deletes by array index (`native/ui/studio_controller.cpp:360`). Protocol v1 therefore uses `{expectedRevision,index}`; a stale index is **CONFLICT**, never applied to a new revision. Optionally add an unknown-field-friendly persistent cable UUID **only following planner decision**; do not renumber module IDs. Host increments revision on every accepted persisted-content mutation (including move and parameter); meter/status events do **not** increment document revision. If multiple clients connect, require a single editing lease or reject conflicting edits.

**Save/load security:** the browser never gives the host an arbitrary filesystem pathname. A privileged shell/file-dialog broker supplies a short-lived, purpose-bound opaque `handle` (`file.pick`), validated by the host. An embedded WebSocket client must be authenticated and the path grant must be bound to the authenticated session. Canonical native v2 is written via atomic temp/rename/sync strategy (Windows/Pi behavior checked separately), with a non-overwriting backup for v1. For downloadable browser operation without shell access, `patch.export` returns a v2 object for a UI-directed download; importing raw bytes must be an explicit size-bounded future command, not an implicit URL fetch.

## 4. Proposed host–web protocol

### 4.1 Transport decision and threat model

| Transport | Benefits | Risks / work | Recommendation |
|---|---|---|---|
| **Tauri IPC / command bridge**, native sidecar or C++ command adapter | No listening socket; privileged file pick/grant; packaged Windows window/lifecycle | Tauri/Rust bridge and sidecar signing, shutdown ordering, ARM64 packaging must be tested | **Default for packaged Windows**; use exactly the same JSON envelopes as WS. |
| **Loopback WebSocket** on `127.0.0.1` and optionally `::1`; one-use high-entropy session token | Qt-free browser/Pi headless access; simple client; local testability | Local malicious pages, DNS rebinding, origin/CSRF, port collisions, token exfiltration, process/socket lifecycle | **Optional host adapter**, never bind `0.0.0.0`; require explicit shell/owner enablement. |
| **HTTP RPC + SSE** | Browser native event stream | Two channels/auth surfaces and harder binary future | Defer; no current need. |

Recommended WebSocket handshake: create **256-bit random** token at host startup, never in URL/query, logs, patch or diagnostics; bootstrap only from the trusted shell; refuse every command/event until a short-lived authentication handshake (token in first frame or bridge-established session), enforce exact allowed `Origin` and `Host` and loopback peer check, cap unauthenticated connections (e.g. 4) and enforce ~3 s auth deadline. Rotate token on host restart; localhost is **not** an authentication boundary. Tauri message transport must apply the same op/argument/permission rules, not a privileged second API. **Never** open MIDI/audio devices automatically in tests, even on loopback (`AGENTS.md`; `native/ui/main.cpp:30-42`; `native/ui/studio_controller.h:44-54`).

Wire: UTF-8 JSON, one message/frame, `v=1`, `type=command|reply|event`, unique client `id` for commands and correlated replies, `op` and `args/result/data`. `expectedRevision` required **by host policy** for persisted-content edits (`module.*,cable.*,parameter.set,output.select,patch.reset`) even though it is optional at the schema envelope level (read-only/getter and lifecycle methods omit it). Reply carries `ok,revision` and exactly one of `result` or `error`; event carries session `epoch` + monotone `seq` + current document `revision`. On reconnect, discard stale `epoch`, request `state.get` and subscribe again. Bootstrap `session.hello` negotiates protocol 1, ABI 2 and capability list; unknown ops, unknown major version and unsupported optional features fail explicitly.

Examples:

```json
{"v":1,"type":"command","id":"q-101","op":"parameter.set","expectedRevision":42,"args":{"node":5,"parameter":0,"value":0.65}}
{"v":1,"type":"reply","id":"q-101","op":"parameter.set","ok":true,"revision":43,"result":{"accepted":true,"value":0.65,"display":"0.65"}}
{"v":1,"type":"event","op":"parameter.changed","seq":911,"epoch":2,"revision":43,"data":{"node":5,"parameter":0,"value":0.65,"display":"0.65","revision":43}}
{"v":1,"type":"reply","id":"q-102","op":"module.remove","ok":false,"revision":43,"error":{"code":"AUDIO_RUNNING","message":"Stop playback to edit topology","retryable":true}}
```

### 4.2 Operation mapping — all inventory items

Each property in table 1A is included with the **same camelCase key and concrete shape** in `state.get` / `session.ready.data.state`; read-only projections are exposed via `catalog.list` and `document.get`. The `status.changed` event updates running/ready/dirty/errors/device/MIDI selection properties, `meters.changed` updates peak/CPU/underruns/diagnostic/midiCounters, `document.changed` replaces the graph projection, and `parameter.changed` patches one accepted value/display. Thus all 24 property reads and all four signals have explicit protocol equivalents. `palette`/`sampleRate`/`blockSize` are treated as session metadata and refreshed only after catalogue/device/engine reconfiguration.

| Current method | Command op | `args` shape | Successful reply `result` | Event(s) |
|---|---|---|---|---|
| `setMidiAuthorized` | `midi.authorize` | `enabled:boolean` | `accepted` | `status.changed` |
| `setMidiEndpoint` | `midi.endpoint.set` | `name,systemName:string;enabled:boolean` | `accepted` | `status.changed` |
| `refreshMidi` | `midi.refresh` | `{}` | `endpoints[]` | `status.changed` |
| `diagnostics` | `diagnostics.get` | `{}` | counter/status object | none |
| `saveDiagnostics` | `diagnostics.save` | `handle:string` | `accepted,name` | `status.changed` |
| `setParameter` | `parameter.set` | `node:uint32;parameter:uint;value:number` | `accepted,value,display` | `parameter.changed`, `status.changed` |
| `moveModule` | `module.move` | `node:uint32;x,y:number` | `accepted` | `document.changed`, `status.changed` |
| `addModule` | `module.add` | `moduleId:string` | `accepted,nodeId:uint32` | `document.changed` |
| `removeModule` | `module.remove` | `node:uint32` | `accepted,revision` | `document.changed` |
| `connectPorts` | `cable.connect` | `from:{module,port};to:{module,port}` | `accepted,revision` | `document.changed` |
| `disconnectCable` | `cable.disconnect` | `index:uint` + mandatory `expectedRevision` | `accepted,revision` | `document.changed` |
| `startAudio` | `audio.start` | `{}` | `accepted` | `status.changed` |
| `stopAudio` | `audio.stop` | `{}` | `accepted` | `status.changed`, `meters.changed` |
| `setGate` | `gate.set` | `on:boolean` | `accepted` | metrics only |
| `note` | `note.send` | `pitch:0..127;on:boolean` | `accepted` | metrics only |
| `panic` | `panic` | `{}` | `accepted` | `status.changed` |
| `setDevice` | `device.set` | `id:int` | `accepted` | `status.changed` |
| `refreshDevices` | `device.refresh` | `{}` | `devices[]` | `status.changed` |
| `selectOutput` | `output.select` | `node:uint32` | `accepted,revision` | `document.changed` |
| `savePatch` | `patch.save` | `handle:string` | `accepted,name` | `status.changed` |
| `loadPatch` | `patch.load` | `handle:string` | `accepted,revision,sourceVersion` | `document.changed`, `status.changed` |
| `renderDemo` | `render.demo` | `handle:string` | `accepted,jobId` | `render.completed` (new asynchronous host event) |
| `resetPatch` | `patch.reset` | `{}` | `accepted,revision` | `document.changed` |

Supplementary commands `session.hello, state.get, catalog.list, document.get, file.pick, patch.export` exist to replace QML context initialization, privilege mediation and browser download. The **canonical v2 patch** returned by `patch.export` must pass `docs/specs/patch_v2_draft.schema.json` and semantic checks; `document.get` is intentionally a UI projection, **not** a lossless save source.

### 4.3 Errors, revision semantics, ordering

Errors are structured `{code,message,retryable,details?}`. Codes: `INVALID_ARGUMENT` (type, NaN/Inf, range), `NOT_FOUND` (node/port/endpoint), `CONFLICT` (stale revision/lease), `AUDIO_RUNNING`, `CLEANUP_PENDING`, `NOT_READY`, `CAPACITY` (ingress full), `UNAUTHORIZED` (transport/session), `PERMISSION_DENIED` (file/device scope), `DEVICE_UNAVAILABLE`, `UNSUPPORTED_FORMAT`, `PATCH_TOO_LARGE`, `PATCH_INVALID`, `IO_ERROR`, `TIMEOUT`, `INTERNAL`. No exception text, local absolute paths or token bytes are reflected to untrusted clients. Errors are terminal per request: no silent success for current `void` methods that reject invalid operations. On `CAPACITY` for note-off/gate-off, guarantee release via reserved ingress priority or request emergency panic; fail safely, never silently drop a release (current controller stops audio on nonparameter queue-full: `native/ui/studio_controller.cpp:362-375`).

Serialize mutations on the **control owner thread**. Assign reply `revision` after acceptance; emit matching event **after** the document swap; preserve command order per client, but do not require cross-client global message arrival order beyond `seq`. Parameter commands may be coalesced only **before acceptance** for same node/parameter and must never coalesce note-off, panic, topology or patch saves. Any retry of a timed-out command may be ambiguous: retain a bounded per-session request-ID reply cache (e.g. 256 recent IDs, 60 s TTL), or require a fresh state snapshot before resubmission.

### 4.4 Event rate, flow control and hard limits

**Today:** only a 50 ms meter timer (20 Hz) and no engine scope stream in the current QObject API (`native/ui/studio_controller.cpp:92-103`, `native/ui/studio_controller.h:19-42`). **Future option:** 30 or 60 Hz **aggregated meters**; a scope requires a separately prepared bounded ring/tap and is **not present now**. Example budget: one ~400-byte meter frame at 60 Hz ≈ **24 KB/s/client** before WebSocket overhead; a hypothetical 2-channel × 256 samples × 2-byte downsampled scope at 30 Hz ≈ **30 KiB/s/client raw payload**, ≈ **60 KiB/s** if 60 Hz, excluding JSON/base64 overhead. Actual rates and serialization sizes require measurement once implemented.

Apply per-subscriber rate caps and a **single-slot latest-value overwrite buffer** for meters; allow at most 2 queued scope frames (drop oldest), separate from a bounded **non-droppable control-event** queue. Suggested starting quotas: **4 MiB max serialized message** (allows encoded 2 MiB patch plus envelope), **64 KiB max uncompressed scope payload**, **256 KiB outbound bytes/client** and **128 pending command IDs/client**; enforce bounds *before* parse/allocation. If a client falls behind: coalesce meter updates, drop scope samples with a `stream.gap` count, disconnect on sustained control-event backlog, never block the callback. Client can request `meters=off|30|60` through an eventual subscription extension; do not send all telemetry to all clients by default. A meter read on a control worker uses atomics/safe published snapshots; it must **never** acquire an audio-thread lock.

### 4.5 Thread and ownership proof obligations

| Thread | May do | Must never do |
|---|---|---|
| **IPC/WebSocket I/O** | Authenticate, enforce frame limits, decode/validate, queue commands, enqueue reply/event serialization | Call engine graph/editing methods, enumerate devices, block audio thread |
| **Single host control owner** | Own canonical patch/revision, catalogue, graph staging, permission and device state, file grant validation; serialize edits; dequeue telemetry snapshots; start/stop producers and streams | Run on PortAudio callback; hold mutex through callback join while callback needs it |
| **Offline render/file worker** | Prepared separate engine; bounded WAV production, atomic writes, background progress | Replace or borrow the live engine without ownership transfer |
| **MIDI/OSC producers** | Bounded per-producer ingress rings; separate authorization and endpoint lifecycle | Call arbitrary JSON/file UI routines on input callback; enqueue unbounded events |
| **PortAudio realtime callback** | Consume preprepared buffers/ingress, call `app_engine_render_channels`/`app_engine_render_live`, update atomics | Parse JSON, touch Qt/Tauri/WebSocket, lock, allocate, perform I/O, throw/log, await UI acknowledgement |

**Existing evidence and limits:** Controller topology commit rejects running or callback-owned engines before rebuilding (`native/ui/studio_controller.cpp:289-307`). `AudioDevice` disables callbacks and waits on an `inFlight` counter before relinquishing the borrowed engine, retaining state if stop cannot prove quiescence (`native/ui/audio_device.cpp:102-115,145-160`). The existing RT callback uses a pre-sized scratch vector, atomic counters and renders via `app_engine_render_live` (`native/ui/audio_device.cpp:135-140,173-200`). The Qt-free PortAudio adapter uses preallocated stereo scratch and a render-only callback, and retains engine memory on catastrophic stop failure (`native/io/audio/portaudio_service.cpp:54-67,85-91`). `app_engine_enqueue` translates control events to engine2's producer-0 ingress, while `app_engine_render_live` only renders and publishes atomic peak/blocks (`native/control/app_engine.cpp:72-100`). MIDI delivery feeds separately reserved producer 1 and panics on overflowed release (`native/control/app_engine.cpp:24-25`, `native/control/midi_host.cpp:42-69`).

These paths **support** the separation, but **do not prove** an unimplemented host is realtime-safe. The current Qt callback contains `try/catch` around render (`native/ui/audio_device.cpp:185-186`); this is a **review item** against the project's “no exceptions in process/event/reset” rule, not a pattern to replicate. Proof for the replacement requires instrumented callback allocation counters, lock/syscall/log probes, bounded worst-case render tests, sanitizer runs, callback shutdown race tests, and source review of every transitive render call. No live-device proof is claimed here.

## 5. JSON Schema (Draft 2020-12): exact wire envelopes

This is a **proposed** schema for protocol v1. It describes every inventory property via `state.get` and every callable via its operation. The conditional schemas enforce argument/result shapes per `op`, including empty args. It is **not** the persisted patch schema; `patch.export.result.patch` has an envelope shape and still requires independent validation using `docs/specs/patch_v2_draft.schema.json` plus native semantic checks. JSON Schema does **not** enforce finite IEEE-754 values, unique instance IDs, graph validity, revision monotonicity, device safety, filesystem grants or stable ID catalogs; validate those in host code. Security policy makes `expectedRevision` mandatory for mutations as noted above, although the envelope schema permits it on any command.

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://wirescape.invalid/schema/studio-host-protocol-v1.json",
  "title": "Wirescape headless Studio host messages — proposal v1",
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
        "id": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
        },
        "op": {
          "enum": [
            "session.hello",
            "state.get",
            "catalog.list",
            "document.get",
            "diagnostics.get",
            "diagnostics.save",
            "midi.authorize",
            "midi.endpoint.set",
            "midi.refresh",
            "parameter.set",
            "module.move",
            "module.add",
            "module.remove",
            "cable.connect",
            "cable.disconnect",
            "audio.start",
            "audio.stop",
            "gate.set",
            "note.send",
            "panic",
            "device.set",
            "device.refresh",
            "output.select",
            "patch.save",
            "patch.load",
            "render.demo",
            "patch.reset",
            "file.pick",
            "patch.export",
            "session.authenticate",
            "output.routes.set",
            "stream.subscribe",
            "stream.unsubscribe"
          ]
        },
        "type": {
          "const": "command"
        },
        "args": {
          "type": "object"
        },
        "expectedRevision": {
          "type": "integer",
          "minimum": 0
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
                "type": "object",
                "properties": {
                  "minVersion": {
                    "const": 1
                  },
                  "maxVersion": {
                    "const": 1
                  },
                  "client": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "minVersion",
                  "maxVersion",
                  "client"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "state.get"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "catalog.list"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "document.get"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "handle": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "handle"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
              }
            },
            "required": [
              "args"
            ]
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
                "$ref": "#/$defs/midiEndpoint"
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "oneOf": [
                  {
                    "type": "object",
                    "properties": {
                      "node": {
                        "$ref": "#/$defs/nodeId"
                      },
                      "parameter": {
                        "type": "integer",
                        "minimum": 0
                      },
                      "value": {
                        "type": "number"
                      }
                    },
                    "required": [
                      "node",
                      "parameter",
                      "value"
                    ],
                    "additionalProperties": false
                  },
                  {
                    "type": "object",
                    "properties": {
                      "node": {
                        "$ref": "#/$defs/nodeId"
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
                      "node",
                      "parameterId",
                      "value"
                    ],
                    "additionalProperties": false
                  }
                ]
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "node": {
                    "$ref": "#/$defs/nodeId"
                  },
                  "x": {
                    "type": "number"
                  },
                  "y": {
                    "type": "number"
                  }
                },
                "required": [
                  "node",
                  "x",
                  "y"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "moduleId": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "moduleId"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "node": {
                    "$ref": "#/$defs/nodeId"
                  }
                },
                "required": [
                  "node"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "cable.connect"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "from": {
                    "$ref": "#/$defs/endpoint"
                  },
                  "to": {
                    "$ref": "#/$defs/endpoint"
                  },
                  "kind": {
                    "enum": [
                      "signal",
                      "modulation",
                      "event"
                    ]
                  },
                  "amount": {
                    "type": "number",
                    "minimum": -1,
                    "maximum": 1
                  },
                  "feedback": {
                    "type": "boolean"
                  },
                  "stereo_pair": {
                    "type": "string"
                  }
                },
                "required": [
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
                  },
                  {
                    "if": {
                      "properties": {
                        "kind": {
                          "enum": [
                            "signal",
                            "event"
                          ]
                        }
                      },
                      "required": [
                        "kind"
                      ]
                    },
                    "then": {
                      "not": {
                        "required": [
                          "amount"
                        ]
                      }
                    }
                  },
                  {
                    "if": {
                      "not": {
                        "required": [
                          "kind"
                        ]
                      }
                    },
                    "then": {
                      "not": {
                        "required": [
                          "amount"
                        ]
                      }
                    }
                  }
                ]
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "cable.disconnect"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "index": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "index"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "gate.set"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
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
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "note.send"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
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
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "panic"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "device.set"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "id": {
                    "type": "integer"
                  }
                },
                "required": [
                  "id"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "device.refresh"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "output.select"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "node": {
                    "$ref": "#/$defs/nodeId"
                  }
                },
                "required": [
                  "node"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "handle": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "handle"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "handle": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  },
                  "discardUnsaved": {
                    "type": "boolean"
                  }
                },
                "required": [
                  "handle"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "handle": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "handle"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "discardUnsaved": {
                    "type": "boolean"
                  }
                },
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {
                  "purpose": {
                    "enum": [
                      "patch.load",
                      "patch.save",
                      "render.demo",
                      "diagnostics.save"
                    ]
                  }
                },
                "required": [
                  "purpose"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
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
                "type": "object",
                "properties": {},
                "required": [],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "session.authenticate"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "token": {
                    "type": "string",
                    "minLength": 32,
                    "maxLength": 512
                  }
                },
                "required": [
                  "token"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "output.routes.set"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "outputs": {
                    "type": "array",
                    "items": {
                      "type": "object",
                      "properties": {
                        "module": {
                          "$ref": "#/$defs/nodeId"
                        },
                        "port": {
                          "type": "string",
                          "minLength": 1,
                          "maxLength": 256
                        },
                        "bus_channel": {
                          "type": "integer",
                          "minimum": 0,
                          "maximum": 15
                        }
                      },
                      "required": [
                        "module",
                        "port",
                        "bus_channel"
                      ],
                      "additionalProperties": false
                    },
                    "maxItems": 16
                  }
                },
                "required": [
                  "outputs"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "stream.subscribe"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
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
                  "source": {
                    "type": "object",
                    "properties": {
                      "node": {
                        "$ref": "#/$defs/nodeId"
                      },
                      "port": {
                        "type": "string",
                        "minLength": 1,
                        "maxLength": 256
                      },
                      "channels": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": 2
                      },
                      "points": {
                        "type": "integer",
                        "minimum": 16,
                        "maximum": 512
                      }
                    },
                    "required": [
                      "node",
                      "port"
                    ],
                    "additionalProperties": false
                  }
                },
                "required": [
                  "stream",
                  "hz"
                ],
                "additionalProperties": false,
                "allOf": [
                  {
                    "if": {
                      "properties": {
                        "stream": {
                          "const": "scope"
                        }
                      },
                      "required": [
                        "stream"
                      ]
                    },
                    "then": {
                      "required": [
                        "source"
                      ]
                    }
                  },
                  {
                    "if": {
                      "properties": {
                        "stream": {
                          "const": "meters"
                        }
                      },
                      "required": [
                        "stream"
                      ]
                    },
                    "then": {
                      "not": {
                        "required": [
                          "source"
                        ]
                      }
                    }
                  }
                ]
              }
            },
            "required": [
              "args"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "stream.unsubscribe"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "args": {
                "type": "object",
                "properties": {
                  "stream": {
                    "enum": [
                      "meters",
                      "scope"
                    ]
                  }
                },
                "required": [
                  "stream"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "args"
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
        "id": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
        },
        "op": {
          "enum": [
            "session.hello",
            "state.get",
            "catalog.list",
            "document.get",
            "diagnostics.get",
            "diagnostics.save",
            "midi.authorize",
            "midi.endpoint.set",
            "midi.refresh",
            "parameter.set",
            "module.move",
            "module.add",
            "module.remove",
            "cable.connect",
            "cable.disconnect",
            "audio.start",
            "audio.stop",
            "gate.set",
            "note.send",
            "panic",
            "device.set",
            "device.refresh",
            "output.select",
            "patch.save",
            "patch.load",
            "render.demo",
            "patch.reset",
            "file.pick",
            "patch.export",
            "session.authenticate",
            "output.routes.set",
            "stream.subscribe",
            "stream.unsubscribe"
          ]
        },
        "type": {
          "const": "reply"
        },
        "ok": {
          "type": "boolean"
        },
        "result": {
          "type": "object"
        },
        "error": {
          "$ref": "#/$defs/error"
        },
        "revision": {
          "type": "integer",
          "minimum": 0
        }
      },
      "required": [
        "v",
        "type",
        "id",
        "op",
        "ok",
        "revision"
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
                "type": "object",
                "properties": {
                  "protocol": {
                    "const": 1
                  },
                  "engineAbi": {
                    "const": 2
                  },
                  "capabilities": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    },
                    "maxItems": 4096
                  },
                  "sessionId": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "protocol",
                  "engineAbi",
                  "capabilities",
                  "sessionId"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "state.get"
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
                "$ref": "#/$defs/snapshot"
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "catalog.list"
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
                          "type": "string"
                        },
                        "description": {
                          "type": "string"
                        }
                      },
                      "required": [
                        "id",
                        "name",
                        "description"
                      ],
                      "additionalProperties": false
                    },
                    "maxItems": 4096
                  }
                },
                "required": [
                  "palette"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "document.get"
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
                "type": "object",
                "properties": {
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "modules": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/moduleView"
                    },
                    "maxItems": 64
                  },
                  "connections": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/connectionView"
                    },
                    "maxItems": 128
                  },
                  "outputNode": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "dirty": {
                    "type": "boolean"
                  },
                  "ready": {
                    "type": "boolean"
                  }
                },
                "required": [
                  "revision",
                  "modules",
                  "connections",
                  "outputNode",
                  "dirty",
                  "ready"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "additionalProperties": true,
                "properties": {
                  "state": {
                    "type": "integer"
                  },
                  "running": {
                    "type": "boolean"
                  },
                  "cleanup_pending": {
                    "type": "boolean"
                  },
                  "device_id": {
                    "type": "integer"
                  },
                  "device": {
                    "type": "string"
                  },
                  "backend": {
                    "type": "string"
                  },
                  "sample_rate": {
                    "type": "number"
                  },
                  "block_frames": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "output_channels": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "underruns": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "callback_faults": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "error": {
                    "type": "string"
                  },
                  "created_utc": {
                    "type": "string"
                  },
                  "scope": {
                    "type": "string"
                  },
                  "patch_ready": {
                    "type": "boolean"
                  },
                  "node_count": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "overload_blocks": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "overload_reason": {
                    "type": "string"
                  },
                  "queue_overflows": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "late_events": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "queued_events": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "rendered_blocks": {
                    "type": "integer",
                    "minimum": 0
                  }
                }
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "name": {
                    "type": "string"
                  }
                },
                "required": [
                  "accepted",
                  "name"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "endpoints": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/midiEndpoint"
                    },
                    "maxItems": 4096
                  }
                },
                "required": [
                  "endpoints"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "value": {
                    "type": "number"
                  },
                  "display": {
                    "type": "string"
                  },
                  "parameterId": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "accepted",
                  "value",
                  "display",
                  "parameterId"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "nodeId": {
                    "$ref": "#/$defs/nodeId"
                  }
                },
                "required": [
                  "accepted",
                  "nodeId"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "accepted",
                  "revision"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "cable.connect"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "accepted",
                  "revision"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "cable.disconnect"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "accepted",
                  "revision"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "gate.set"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "note.send"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "panic"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "device.set"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "device.refresh"
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
                "type": "object",
                "properties": {
                  "devices": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/device"
                    },
                    "maxItems": 4096
                  }
                },
                "required": [
                  "devices"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "output.select"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "accepted",
                  "revision"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "name": {
                    "type": "string"
                  }
                },
                "required": [
                  "accepted",
                  "name"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "sourceVersion": {
                    "enum": [
                      1,
                      2
                    ]
                  }
                },
                "required": [
                  "accepted",
                  "revision",
                  "sourceVersion"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "jobId": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "accepted",
                  "jobId"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "accepted",
                  "revision"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "handle": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  },
                  "displayName": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "handle",
                  "displayName"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
                "type": "object",
                "properties": {
                  "encoding": {
                    "const": "base64"
                  },
                  "data": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 4000000
                  },
                  "byteLength": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 2097152
                  },
                  "sha256": {
                    "type": "string",
                    "pattern": "^[0-9a-f]{64}$"
                  }
                },
                "required": [
                  "encoding",
                  "data",
                  "byteLength",
                  "sha256"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "session.authenticate"
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
                "type": "object",
                "properties": {
                  "authenticated": {
                    "const": true
                  }
                },
                "required": [
                  "authenticated"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "output.routes.set"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "accepted",
                  "revision"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "stream.subscribe"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  },
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
                  }
                },
                "required": [
                  "accepted",
                  "stream",
                  "hz"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "stream.unsubscribe"
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
                "type": "object",
                "properties": {
                  "accepted": {
                    "const": true
                  }
                },
                "required": [
                  "accepted"
                ],
                "additionalProperties": false
              }
            },
            "required": [
              "result"
            ]
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
        "op": {
          "enum": [
            "session.ready",
            "document.changed",
            "parameter.changed",
            "status.changed",
            "meters.changed",
            "stream.gap",
            "render.completed",
            "scope.frame"
          ]
        },
        "seq": {
          "type": "integer",
          "minimum": 0
        },
        "revision": {
          "type": "integer",
          "minimum": 0
        },
        "epoch": {
          "type": "integer",
          "minimum": 0
        },
        "data": {
          "type": "object"
        }
      },
      "required": [
        "v",
        "type",
        "op",
        "seq",
        "revision",
        "epoch",
        "data"
      ],
      "additionalProperties": false,
      "allOf": [
        {
          "if": {
            "properties": {
              "op": {
                "const": "session.ready"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "state": {
                    "$ref": "#/$defs/snapshot"
                  }
                },
                "required": [
                  "state"
                ],
                "additionalProperties": false
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "document.changed"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "modules": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/moduleView"
                    },
                    "maxItems": 64
                  },
                  "connections": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/connectionView"
                    },
                    "maxItems": 128
                  },
                  "outputNode": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "dirty": {
                    "type": "boolean"
                  },
                  "ready": {
                    "type": "boolean"
                  }
                },
                "required": [
                  "revision",
                  "modules",
                  "connections",
                  "outputNode",
                  "dirty",
                  "ready"
                ],
                "additionalProperties": false
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "parameter.changed"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "node": {
                    "$ref": "#/$defs/nodeId"
                  },
                  "parameter": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "value": {
                    "type": "number"
                  },
                  "display": {
                    "type": "string"
                  },
                  "revision": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "parameterId": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  }
                },
                "required": [
                  "node",
                  "value",
                  "display",
                  "revision"
                ],
                "additionalProperties": false,
                "anyOf": [
                  {
                    "required": [
                      "parameter"
                    ]
                  },
                  {
                    "required": [
                      "parameterId"
                    ]
                  }
                ]
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "status.changed"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
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
                  "statusText": {
                    "type": "string"
                  },
                  "errorText": {
                    "type": "string"
                  },
                  "noticeText": {
                    "type": "string"
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
                      "$ref": "#/$defs/device"
                    },
                    "maxItems": 4096
                  },
                  "selectedDevice": {
                    "type": "integer"
                  },
                  "midiEndpoints": {
                    "type": "array",
                    "items": {
                      "$ref": "#/$defs/midiEndpoint"
                    },
                    "maxItems": 4096
                  },
                  "midiAuthorized": {
                    "type": "boolean"
                  },
                  "midiSettingsAvailable": {
                    "type": "boolean"
                  }
                },
                "required": [
                  "running",
                  "ready",
                  "cleanupPending",
                  "statusText",
                  "errorText",
                  "noticeText",
                  "dirty",
                  "patchName",
                  "devices",
                  "selectedDevice",
                  "midiAuthorized",
                  "midiSettingsAvailable"
                ],
                "additionalProperties": false
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "meters.changed"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
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
                    "$ref": "#/$defs/midiCounters"
                  },
                  "frame": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "appliedGeneration": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "peak",
                  "cpuLoad",
                  "underruns",
                  "diagnosticText",
                  "midiCounters",
                  "frame",
                  "appliedGeneration"
                ],
                "additionalProperties": false
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "stream.gap"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "stream": {
                    "enum": [
                      "meters",
                      "scope"
                    ]
                  },
                  "dropped": {
                    "type": "integer",
                    "minimum": 0
                  }
                },
                "required": [
                  "stream",
                  "dropped"
                ],
                "additionalProperties": false
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "render.completed"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "jobId": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  },
                  "ok": {
                    "type": "boolean"
                  },
                  "message": {
                    "type": "string"
                  }
                },
                "required": [
                  "jobId",
                  "ok",
                  "message"
                ],
                "additionalProperties": false
              }
            }
          }
        },
        {
          "if": {
            "properties": {
              "op": {
                "const": "scope.frame"
              }
            },
            "required": [
              "op"
            ]
          },
          "then": {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "node": {
                    "$ref": "#/$defs/nodeId"
                  },
                  "port": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 256
                  },
                  "format": {
                    "const": "f32le-base64"
                  },
                  "channels": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": 2
                  },
                  "points": {
                    "type": "integer",
                    "minimum": 16,
                    "maximum": 512
                  },
                  "frameStart": {
                    "type": "integer",
                    "minimum": 0
                  },
                  "sampleRate": {
                    "type": "integer",
                    "minimum": 1
                  },
                  "samplesBase64": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 16384
                  }
                },
                "required": [
                  "node",
                  "port",
                  "format",
                  "channels",
                  "points",
                  "frameStart",
                  "sampleRate",
                  "samplesBase64"
                ],
                "additionalProperties": false
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
            "INVALID_ARGUMENT",
            "NOT_FOUND",
            "CONFLICT",
            "AUDIO_RUNNING",
            "CLEANUP_PENDING",
            "NOT_READY",
            "CAPACITY",
            "UNAUTHORIZED",
            "PERMISSION_DENIED",
            "DEVICE_UNAVAILABLE",
            "UNSUPPORTED_FORMAT",
            "PATCH_TOO_LARGE",
            "PATCH_INVALID",
            "IO_ERROR",
            "TIMEOUT",
            "INTERNAL",
            "UNSUPPORTED_CAPABILITY"
          ]
        },
        "message": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
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
          "$ref": "#/$defs/midiCounters"
        },
        "midiEndpoints": {
          "type": "array",
          "items": {
            "$ref": "#/$defs/midiEndpoint"
          },
          "maxItems": 4096
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
            "$ref": "#/$defs/moduleView"
          },
          "maxItems": 64
        },
        "connections": {
          "type": "array",
          "items": {
            "$ref": "#/$defs/connectionView"
          },
          "maxItems": 128
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
                "type": "string"
              },
              "description": {
                "type": "string"
              }
            },
            "required": [
              "id",
              "name",
              "description"
            ],
            "additionalProperties": false
          },
          "maxItems": 4096
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
          "type": "number"
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
            "$ref": "#/$defs/device"
          },
          "maxItems": 4096
        },
        "selectedDevice": {
          "type": "integer"
        },
        "sampleRate": {
          "type": "integer",
          "minimum": 1
        },
        "blockSize": {
          "type": "integer",
          "minimum": 1
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
        "blockSize"
      ],
      "additionalProperties": false
    },
    "moduleView": {
      "type": "object",
      "properties": {
        "id": {
          "$ref": "#/$defs/nodeId"
        },
        "moduleId": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
        },
        "title": {
          "type": "string"
        },
        "description": {
          "type": "string"
        },
        "x": {
          "type": "number"
        },
        "y": {
          "type": "number"
        },
        "missing": {
          "type": "boolean"
        },
        "inputs": {
          "type": "array",
          "items": {
            "$ref": "#/$defs/port"
          },
          "maxItems": 4096
        },
        "outputs": {
          "type": "array",
          "items": {
            "$ref": "#/$defs/port"
          },
          "maxItems": 4096
        },
        "parameters": {
          "type": "array",
          "items": {
            "$ref": "#/$defs/parameter"
          },
          "maxItems": 4096
        }
      },
      "required": [
        "id",
        "moduleId",
        "title",
        "description",
        "x",
        "y",
        "missing",
        "inputs",
        "outputs",
        "parameters"
      ],
      "additionalProperties": false
    },
    "midiCounters": {
      "type": "object",
      "properties": {
        "received": {
          "type": "integer",
          "minimum": 0
        },
        "dropped": {
          "type": "integer",
          "minimum": 0
        },
        "malformed": {
          "type": "integer",
          "minimum": 0
        },
        "oversize": {
          "type": "integer",
          "minimum": 0
        },
        "ring_full": {
          "type": "integer",
          "minimum": 0
        },
        "disconnects": {
          "type": "integer",
          "minimum": 0
        },
        "send_fail": {
          "type": "integer",
          "minimum": 0
        },
        "ports": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "name": {
                "type": "string"
              },
              "system_name": {
                "type": "string"
              },
              "received": {
                "type": "integer",
                "minimum": 0
              },
              "online": {
                "type": "boolean"
              }
            },
            "required": [
              "name",
              "system_name",
              "received",
              "online"
            ],
            "additionalProperties": false
          },
          "maxItems": 4096
        }
      },
      "required": [
        "received",
        "dropped",
        "malformed",
        "oversize",
        "ring_full",
        "disconnects",
        "send_fail",
        "ports"
      ],
      "additionalProperties": false
    },
    "connectionView": {
      "type": "object",
      "properties": {
        "fromNode": {
          "$ref": "#/$defs/nodeId"
        },
        "fromPort": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
        },
        "toNode": {
          "$ref": "#/$defs/nodeId"
        },
        "toPort": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
        },
        "kind": {
          "enum": [
            "audio",
            "control",
            "event"
          ]
        },
        "channels": {
          "type": "integer",
          "minimum": 1,
          "maximum": 16
        }
      },
      "required": [
        "fromNode",
        "fromPort",
        "toNode",
        "toPort",
        "kind",
        "channels"
      ],
      "additionalProperties": true
    },
    "parameter": {
      "type": "object",
      "properties": {
        "id": {
          "type": "integer",
          "minimum": 0
        },
        "name": {
          "type": "string"
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
        }
      },
      "required": [
        "id",
        "name",
        "minimum",
        "maximum",
        "value",
        "unit",
        "display"
      ],
      "additionalProperties": false
    },
    "midiEndpoint": {
      "type": "object",
      "properties": {
        "name": {
          "type": "string",
          "minLength": 1,
          "maxLength": 256
        },
        "systemName": {
          "type": "string"
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
    "port": {
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
            "event"
          ]
        }
      },
      "required": [
        "id",
        "label",
        "kind"
      ],
      "additionalProperties": false
    },
    "endpoint": {
      "type": "object",
      "properties": {
        "module": {
          "$ref": "#/$defs/nodeId"
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
    "device": {
      "type": "object",
      "properties": {
        "id": {
          "type": "integer"
        },
        "name": {
          "type": "string"
        }
      },
      "required": [
        "id",
        "name"
      ],
      "additionalProperties": false
    },
    "nodeId": {
      "type": "integer",
      "minimum": 1,
      "maximum": 4294967295
    }
  }
}
```

## 6. Offline host acceptance tests

All tests below use fake audio/MIDI device interfaces, temporary directories, loopback-only sockets when exercising the WS adapter, deterministic time and injected failure points. **Never** open real devices, LAN sockets or microphones. Relevant existing fake-backend/testing seams: `native/ui/audio_device.h:14-39`, `native/control/midi_host.h:10-32`, `native/ui/CMakeLists.txt:32-51`, `native/document/CMakeLists.txt:13-18`.

| # | Host test / assertions | Evidence / desired gate |
|---:|---|---|
| 1 | Enumerate **all 24** snapshot keys, their types and default 5-node/5-cable contents | Assert against `studio_controller.h:19-42` and `studio_controller.cpp:23-42` |
| 2 | Exercise each of **23** RPC operation names with both valid and rejected args; typed replies always correlate ID/op | New schema validation matrix |
| 3 | Emit all **4** legacy-equivalent signals as revisioned events; observe order, no missing parameter accepted value/display | `studio_controller.h:110-114` |
| 4 | Fuzz malformed UTF-8, duplicate keys, huge strings, JSON depth 65, non-objects, decimal IDs, NaN/Inf via native checks, unknown op/version; no process crash | `runtime_patch.cpp:28-36` and host schema |
| 5 | Up to 64 modules / 128 cables and 2 MiB parsed patch succeed/fail at boundaries; reject 2 MiB+1; enforce wire size and budget separately | `runtime_patch.cpp:28-38` |
| 6 | Native v1 fixture upgrades to canonical v2; IDs, mapped parameters and full unknown envelope preserved; original bytes written to non-overwriting sibling `.v1.json` | `patch_document.cpp:145-203,220-268` |
| 7 | V2 read/edit/save/reload retains unknown top-level/module/config/state/cable/endpoint/device/host-mapping fields; preserves IDs and order | `patch_v2_draft.schema.json:23-110`, `app_patch.cpp:37-48` |
| 8 | Missing module is editable and saveable, but audio.start refuses, without changing source bytes; restoring module returns readiness | `patch_document.cpp:204-218`, `studio_controller.cpp:265-271` |
| 9 | Invalid graph, unsupported version, duplicate module/cable endpoint, bad modulation amount, out-of-range parameter and unknown stable IDs return precise errors, leave revision/document unchanged | `patch_document.cpp:86-143`, `runtime_patch.cpp:49-76` |
| 10 | Stale `expectedRevision` on cable index, module edits, parameter writes, output selection and reset returns CONFLICT, not a stale mutation | New host CAS invariant |
| 11 | Failed prepare or graph replacement keeps accepted engine/patch and no callbacks reference freed graphs | `studio_controller.cpp:298-307` |
| 12 | Under fake audio Running/Stopping/CleanupPending states, reject graph mutation and load; stop failure quarantines borrowed engine; retry cleanup safely | `audio_device.cpp:102-115,145-160` |
| 13 | Parameter acceptance at limits, enum/discrete values, stable numeric-to-string mapping; live changes enter reserved ingress; display update matches accepted value | `studio_controller.cpp:310-317`, `app_patch.cpp:9-14` |
| 14 | Gate/note press-release/focus-loss simulation, rapid retrigger, MIDI note identity and panic; no stuck note at queue saturation or disconnect | `studio_controller.cpp:362-375,402-425`, `midi_host.cpp:42-69` |
| 15 | Fake audio devices: default selection, refresh, missing stereo, unsupported sample rate, backend initialization/teardown failures, running state | `audio_device.cpp:30-77,117-171` |
| 16 | Fake MIDI: authorization disabled in offline CI, explicit authorization only, duplicate endpoint display names + distinct `system_name`, restore, selected limit **16**, counters | `midi_host.cpp:9-40,108-124` |
| 17 | Session token authentication, malformed Origin/Host, wrong token, expired token, replay, excess connections, unauthenticated timeout and zero LAN bind | New transport security requirements |
| 18 | Tauri bridge/WS behavioral parity, same JSON schema/revisions/errors and file-handle permission checks | New adapter-contract test |
| 19 | Grant wrong purpose/session, path traversal, symlink/rename race, target overwrite failure, disk-full during backup/commit; previous file unchanged | `patch_document.cpp:220-268` |
| 20 | Multi-client edit conflict and single-editor lease; command deduplication after retry; reconnect epoch/snapshot resynchronization | New concurrency contract |
| 21 | 60 Hz telemetry with deliberately stalled receiver; bounded CPU/memory and queue, drop/coalesce latest meter, `stream.gap`, control replies still delivered | New backpressure stress test |
| 22 | Fake scope ring payload/framing at 30/60 Hz and cap; no scope added to audio callback before explicit bounded-tap design | Future optional feature |
| 23 | Offline 4 s mono WAV golden PCM/headers, no overwrite; long render moved off IPC thread, cancellation/worker lifecycle | `studio_controller.cpp:523-539` |
| 24 | Freeze/render fault/panic while host shuts down; stop MIDI producers first, callback join/retention, no use-after-free under ASan/TSan | `audio_device.cpp:106-111,145-160`, `portaudio_service.cpp:85-91` |
| 25 | Allocation, lock and forbidden-I/O hooks around callback and transitive engine2 process/reset; finite bounded event budget and output | `AGENTS.md`; `app_engine.cpp:91-100`; `runtime_patch.cpp:28-76` |
| 26 | Cross-platform Windows and Linux ARM64 compile/package protocol host without Qt; offline renderer equality; no real device side effects | `native/CMakeLists.txt:17,99-109`, `native/engine2/CMakeLists.txt:141-162` |
| 27 | Property-based patch mutation round-trips including empty outputs, inactive modules, feedback edges, event/modulation/stereo attributes without losing unknown fields | `patch_v2_draft.schema.json:55-110` |
| 28 | Regression that `patch.save` after edit does not reserialize or replace a recognized legacy `wirescape.patch`/visual-study file as native without an explicit migration step | `studio_controller.cpp:494-509`, `patch_document.cpp:227-240` |

**Order of work (recommendation):** (1) generate protocol conformance fixtures from this schema and current QObject snapshots, (2) port the preservation-capable document adapter to Qt-free C++17, (3) implement a control-owned host with fake backend, (4) add Tauri and/or authenticated WS adapters, (5) run Windows/Linux ARM64 headless CI and performance checks. A pure documentation PR cannot claim those tests ran.

## Facts versus recommendations: unresolved risks

- **Observed:** QML consumes a v1-shaped model, while native persistence is v2; direct raw UI snapshot serialization is not a safe v2 writer (`native/document/app_patch.cpp:16-48`).
- **Observed:** the Qt-free runtime loader is deliberately strict and cannot preserve unresolved modules while executing (`native/document/runtime_patch.cpp:49-54`, `native/document/runtime_patch.h:5-10`).
- **Observed:** current timer cadence is 20 Hz; 30–60 Hz and scope are proposals, **not existing endpoints** (`native/ui/studio_controller.cpp:92-103`, `native/ui/studio_controller.h:19-42`).
- **Recommended, not proved:** Tauri sidecar packaging, same protocol over WS, brokered file handles, true cross-platform atomic replace and realtime-instrumented queues. No deployment, build timing or measured benchmark result is claimed by this research.

## Decisions needed from the planner

1. **Desktop transport:** (A) Tauri IPC as default + optional authenticated loopback WS, (B) WS-only, (C) Tauri-only. **Recommend A**: package security without sacrificing Pi/browser use.
2. **Editable document seam:** (A) port Qt-backed lossless v2 editor/migrator/writer to C++17+nlohmann, (B) retain QtCore privately in host, (C) execute-only Qt-free host with editing deferred. **Recommend A**, gated by round-trip and backup tests; C is inadequate for today's UI.
3. **Wire versioning:** (A) protocol v1 with explicit `v` and capability negotiation, independent of patch v2/ABI2, (B) reuse patch version, (C) unversioned commands. **Recommend A**.
4. **Cable deletion identity:** (A) revision-guarded array index now, (B) add persistent cable UUID field to v2, (C) match endpoint tuple only. **Recommend A** first; discuss B as additive compatibility work, not an implicit ID change.
5. **Editing while playing:** (A) preserve stopped-only topology changes, allow live parameter/note ingress, (B) stage full live graph swaps, (C) reject all editing during audio. **Recommend A** as current safe baseline.
6. **Meters/scope:** (A) start 30 Hz latest-only meters and postpone scope, (B) 60 Hz meters + bounded scope now, (C) keep 20 Hz indefinitely. **Recommend A** until measured Pi 5 CPU/IPC budgets exist.
7. **File access:** (A) privileged handle-based pick/save grants + optional browser export, (B) accept arbitrary client paths, (C) web downloads only. **Recommend A** to preserve atomic backup and constrain filesystem authority.
8. **Realtime assurance gate:** (A) fake-backend + alloc/lock/I/O probes + sanitizer/ARM64 offline evidence before replacing Qt UI, (B) unit tests only, (C) manual laptop demo only. **Recommend A**; manual devices remain owner-only, not an automated test.
