<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Engine protocol v0 — app ↔ `wirescape-engine` (#228)

**Status:** normative Phase 1 contract for P1B (sidecar), P1C (`RealEngine`) and P1F (Electron shell). **Version:** `v: 0`; incompatible changes require a new protocol version, not silent reinterpretation. Implements [the P1.0 task](../tasks/done/P1_0_PROTOCOL.md) and `docs/PLAN.md` D1, D3–D9. This supersedes the transport and document-ownership proposals in research PR #217 §4: **no WebSocket, HTTP, Tauri or other network socket**.

## 1. Process, transport and limits

```text
renderer → window.wirescape.send(obj) → isolated preload → Electron main
         → child stdin (NDJSON) → wirescape-engine control thread
renderer ← window.wirescape.onMessage(cb) ← preload ← Electron main
         ← child stdout (NDJSON)
                                      child stderr → diagnostic logs only
```

- One UTF-8 JSON **object** per LF-terminated line in each direction; CRLF accepted on input. No BOM, embedded raw LF, duplicate JSON object keys, invalid UTF-8, NaN or Infinity. `stdout` contains protocol lines only; stdout logging is a protocol bug. `stderr` is unconstrained, non-protocol log text and never parsed as a reply.
- **Hard cap:** 2,097,152 UTF-8 bytes *per line*, excluding LF, including JSON framing and escaping. This is also the native patch-byte limit; a patch that cannot fit inside the wire envelope is refused with `limit`, not fragmented or truncated. Count bytes, not JavaScript characters. Discard an overlong line through its next LF with bounded memory. JSON nesting depth ≤64. Reject malformed lines with one `invalid` reply and keep processing the next line.
- A single engine control executor processes accepted commands in receive order. No more than **256** accepted commands may await execution; when full, parse bounded subsequent lines and reply `busy` without enqueuing. Device and telemetry notifications cannot block replies. Never read, parse or serialize JSON on an audio/MIDI callback; no allocation, mutex, logging or file access there.
- On **stdin EOF** (including an unfinished final line), discard the unfinished line, stop and join OSC ingress, then stop MIDI ingress and audio, join callbacks and exit **0** after clean teardown. `SIGTERM`/crash is not an EOF success. Main drains complete stdout lines, observes process exit and emits `app.engineExited`.
- Main enables `contextIsolation: true`, `nodeIntegration: false`; the preload exposes **only** the five members of `window.wirescape` specified in §3 (`send`, `onMessage`, `openPatch`, `savePatch`, `platform`) via `contextBridge`. Preload validates protocol objects, serializability, size and protocol version before sending over Electron IPC. Main is the sole owner of the child process, serializes one line per command, and forwards accepted engine lines **unchanged** to the renderer (no reparsing/reformatting), except its own `app.*` messages. The dialog methods are app-owned IPC calls, not stdin commands. No filesystem, shell, Node or raw child streams are exposed.
- Main bounds stdin/IPC pending writes and forwards `busy` when the 256-command pending limit is reached. Main drops/coalesces only superseded status/telemetry; it never drops command replies, `hello`, `devices` or `app.engineExited`. On a broken pipe, pending calls are refused as `unavailable`. No HTTP or WebSocket listener is introduced; the separately consent-gated OSC In module may bind UDP as specified in §7 and §9.

## 2. Envelope, sessions, errors

Each command is `{"v":0,"id":"r1","type":"loadGraph", ...}`; top-level `id` is **reserved exclusively for request correlation**, never for a target device/module. It is a nonempty printable ASCII string of at most 64 bytes, unique among outstanding commands and not reused within the engine session. Device selection commands use `deviceId` as their payload key, while `Device.id` inside nested catalogue/status/result objects remains unchanged. Duplicate keys are invalid at every JSON object nesting level. Engine replies, with the **same request id**, have exactly one of:

```json
{"v":0,"session":"s1","re":"r1","ok":true,"gen":"1"}
{"v":0,"session":"s1","re":"r1","ok":false,"code":"not-found","message":"No such module","gen":"1"}
```

- `result` (a JSON object) is present only when a command below specifies returned data. Both success and refusal always include `gen`. `gen` is a **base-10 nonnegative decimal string** (64-bit graph generation, not a JS float), incremented once for each committed graph replacement. It does not advance for event-queue commands, queries, device selection or a rejected edit. It starts at `"0"` for each engine process.
- `session` is a fresh opaque process-instance token (ASCII) announced by `hello` and repeated on **every** engine reply/push. Main drops late lines from an exited child. `RealEngine` ignores messages from a noncurrent session; it does not compare generations across process sessions. Commands are sent only after `hello`; no command needs a session field because only one child receives stdin.
- Malformed lines lacking an identifiable `id` use `re:null`, `code:"invalid"`, `gen` at the current generation; oversized lines use `code:"limit"`. Unsupported `v`/`type`, invalid field types, invalid identifiers and non-finite numbers are `invalid`. Each parsed command gets exactly one terminal reply; notifications may interleave, but replies to ordered commands retain order.
- Error `code` is one of the UI's `EngineError` codes **`unknown-module`, `kind-mismatch`, `duplicate`, `not-found`, `refused`**, plus **`invalid`, `busy`, `limit`, `unavailable`**. `message` is a safe, nonempty human-readable string (no arbitrary path or secret). `unknown-module` means an unavailable native descriptor; `kind-mismatch` is event vs signal; `duplicate` means duplicate IDs/cables or ambiguous device names; `not-found` means unknown existing module/port/parameter/device; `refused` means a valid but disallowed operation; `limit` covers bounds (including 64 nodes/128 cables); `unavailable` covers absent capability/device/sidecar.
- Engine pushes are `{"v":0,"session":"s1","type":"hello"|"status"|"telemetry"|"devices",...}`. Each non-`hello` engine push includes current `gen`. Pushes never carry `re`; replies never carry `type`. Unrecognized optional fields are ignored by v0 clients. An unknown protocol version is **not** accepted under a v0 fallback.

## 3. App-owned messages, lifecycle

`{"v":0,"id":"a1","type":"app.restartEngine"}` is intercepted by **main**, never sent to the engine. Main replies `{"v":0,"re":"a1","ok":true,"gen":"0"}` when restart is initiated, sends EOF/terminates the old child with bounded cleanup, and starts a new one with the same authorized startup flags. Main announces any child exit with `{"v":0,"type":"app.engineExited","exitCode":null|integer,"signal":null|string,"expected":boolean}` (its own push; no engine session). A failed restart returns an app-owned `unavailable` reply instead. An unexpected child exit does not imply a successful restart; the UI may request one. Main never forwards `app.*` to engine stdin.

Native File menu commands are **main-owned intent notifications**, not engine messages: `{"v":0,"type":"app.menuOpenPatch"}`, `{"v":0,"type":"app.menuSavePatch","saveAs":false}` and `{"v":0,"type":"app.menuSavePatch","saveAs":true}`. They arrive through `onMessage` without `id`, `re`, `session` or `gen`; they never enter engine stdin. On Open, the renderer invokes `openPatch()` and replaces its editable document **only on success**. On Save, the renderer serializes its current lossless document text and calls `savePatch(text,{saveAs})`; `null` means the user cancelled. Renderer-initiated toolbar commands call those same bridge methods directly. Main never invents or edits the document text.

On every new engine `hello`, the UI invalidates old-session pending edits/replies, calls `loadGraph` using its **own current editable document**, then resumes incremental commands only after that load is acknowledged. Engine settings survive restarts separately; the engine does not store or overwrite the UI patch. The app must show an engine-disconnected state between exit and successful `hello`/`loadGraph`. The first `hello` precedes other engine pushes.

### Electron preload bridge (P1F, non-engine IPC)

The **entire** exposed `contextBridge` object is the following five-member API. The renderer never receives Node primitives or an arbitrary filesystem path. Native File menu actions emit app-owned intents to the renderer (defined below), which invokes these bridge methods. File operations are **not** v0 engine commands; only menu **intents** use `app.*` notifications.

```ts
interface WirescapeBridge {
  send(obj: Record<string, unknown>): void;
  onMessage(cb: (message: Record<string, unknown>) => void): () => void;
  openPatch(): Promise<{ text: string; name: string } | null>;
  savePatch(text: string, options?: { saveAs?: boolean }): Promise<{ name: string } | null>;
  readonly platform: 'linux' | 'win32' | 'darwin';
}
```

- `send` submits one v0 command (including `app.restartEngine`) through bounded, validated IPC; it never opens a file or returns a reply Promise. `onMessage` registers a callback for decoded protocol replies/pushes and app-owned messages, returning an idempotent unsubscribe function. Subscription is removed on window teardown. The renderer must not invent a `Device.id` as the top-level request `id`.
- `openPatch()` shows Electron's native **Open patch** file dialog, reads the chosen UTF-8 patch **whole**, and resolves `{text, name}`, where `name` is the display basename only. Cancellation resolves `null`, with no document change. Main retains the dialog-selected absolute path privately for subsequent **Save**; it never returns the path to the renderer. The UI remains the sole editable-document owner.
- `savePatch(text, {saveAs:false})` writes the supplied exact UTF-8 document text to the last path selected by Open/Save in **this window**. If no such path exists, or `saveAs:true`, main shows a native **Save patch** dialog and remembers the chosen path after success. `saveAs` defaults to `false`. Success resolves `{name}` (display basename), cancellation resolves `null` and leaves the existing document/path unchanged. Write via same-directory temporary file plus atomic replacement; do not rewrite the document, strip unknown keys or pass it through the engine.
- Both file methods enforce the existing 2 MiB UTF-8 limit on file bytes, reject non-UTF-8 input, invalid arguments and I/O failures with a rejected Promise carrying a sanitized error string, and do not silently convert failure into cancellation. No renderer-supplied path or generic read/write API exists. The platform property is the immutable Electron main-process OS identifier, for UI presentation/shortcut conventions only; it does **not** grant access to `process`.

## 4. Commands (one terminal reply per request)

Wire `type` is the `EngineClient` method name, except `ccMap.set`. Numbers are finite JSON numbers, module instance IDs are positive uint32, parameter and port IDs are stable strings from the catalogue. `Connection` is the exact UI object `{kind:"signal"|"event"|"modulation",from:{module,port},to:{module,port},amount?}`; modulation `amount` is in `[-1,1]`. Unknown document fields are preserved, never rewritten by these operations.

**Apply class:** `live` = enqueue on the existing bounded event queue (accepted ≠ already rendered); `swap` = construct/validate candidate off-thread, then live replan, else D5: mute → the running callback adopts the prepared graph at a block boundary (no device reopen) → fade in; only a stopped or lost stream restarts; `control` = ordered non-RT service work without graph replacement; `query` = ordered read. **Latency:** `block` = delivered no earlier than next rendered block, no guaranteed wall time while stopped; `edit` = D5 graph-interruption gap ≤150 ms on a functioning audio device (processing time is separate); `device` = device reopen/retry may exceed 150 ms; `immediate` = ordered control-thread answer, no audio deadline.

| `type` | Payload in command (besides envelope) | Apply | Latency |
|---|---|---|---|
| `loadGraph` | `patch: PatchV2` (entire editable v2 JSON object) | swap, atomic | edit |
| `addModule` | `moduleId:uint32`, `moduleType:string`, `params?:{paramId:number}` | swap | edit |
| `removeModule` | `moduleId:uint32` (remove its attached cables) | swap | edit |
| `connect` | `connection:Connection` | swap | edit |
| `disconnect` | `connection:Connection` (identity: kind/from/to; amount ignored) | swap | edit |
| `setParam` | `moduleId:uint32`, `paramId:string`, `value:number` (parameter's native units) | live | block |
| `setModAmount` | `connection:Connection` (identity: kind/from/to; nested amount ignored), `amount:number` in [-1,1] (top-level wins) | swap | edit |
| `noteOn` | `note:integer` 0..127, `velocity:number` 0..1 | live | block |
| `noteOff` | `note:integer` 0..127 (never silently drop) | live, priority | block |
| `panic` | `{}` | live, priority release-all | block |
| `listAudioDevices` | `{}`; result `{devices: Device[]}` | query | immediate |
| `selectAudioDevice` | `deviceId:string` (audio `Device.id`; not the request `id`) | control / audio reopen | device |
| `listMidiDevices` | `{}`; result `{devices: Device[]}` | query | immediate |
| `setMidiEnabled` | `deviceId:string` (from `Device.id`), `on:boolean` | control / MIDI subscriptions | device |
| `midiLearn` *(capability-gated)* | `armed:boolean`; result `{armed:boolean}` | control / transient observation | immediate |
| `setSampleRate` *(optional)* | `hz:integer` in {44100, 48000, 96000}, supported by selected output | control / stream-only restart, reprepare | device |
| `setBufferFrames` *(optional)* | `frames:integer` in {64, 128, 256, 512, 1024} | control / stream-only restart, reprepare | device |
| `setOsc` *(optional, **unavailable in v0**)* | `cfg:{enabled?,port?,allowNetwork?}` | refused `unavailable`; no socket | immediate |
| `ccMap.set` | `moduleId:uint32`, `row:integer` 0..7, `mapping:CcRow` | swap/config reprepare | edit |

`CcRow` contains `channel` integer 0 (omni) or 1..16, `controller` integer 0..127, optional `fourteen_bit`/`bipolar` booleans and `smoothing_ms` finite 0..2000. `ccMap.set` replaces/adds **only** the specified `midi.cc_to_cv` module's `config.rows[row]`, preserving other rows and module document fields in the UI's source document. Rows cannot have holes (row ≤ current length); max 8 rows per module. If full, MIDI Learn creates another `midi.cc_to_cv` node via `addModule` and commits its mapping there. The engine's execution copy gets these rows; **`ui.ccMap` is display metadata, never the DSP configuration**. `ccMap.set` refuses `not-found` for a non-CC node and `limit` for row overflow. The UI must update its own `config.rows` on confirmed edits; the engine never sends back a document.

A successful `setSampleRate` or `setBufferFrames` runs wholly on the control thread: validate the allowed value and device support, stop only the stream, rebuild/prepare the current runnable graph at the new rate/block off the audio thread (D5 path), release held notes and restart module DSP state, then restart the stream. The editable document and selected MIDI subscriptions are retained. On failure, restore the old rate/block and running stream and reply `refuse` with code `invalid` (out-of-set) or `refused`/`rate-unsupported` (unsupported rate), or `unavailable` (other device/restart refusal); never persist a rejected setting. Only after the new stream is running may the engine persist `audio.sampleRate`/`audio.bufferFrames` in `settings.json`; `--rate` and `--block` override those stored values for the current process. The success reply is emitted **after** start; the next scheduled (at most 100 ms) `status` push follows with the new `sampleRate`, `bufferFrames` and estimated `latencyMs` (same session, current generation). Example successful stream restart:
```json
{"v":0,"id":"cfg1","type":"setBufferFrames","frames":128}
{"v":0,"session":"s1","re":"cfg1","ok":true,"gen":"3"}
{"v":0,"session":"s1","type":"status","gen":"3","status":{"audio":{"state":"running","deviceId":"USB Stereo","deviceName":"USB Stereo","sampleRate":48000,"bufferFrames":128,"latencyMs":5.333333333333333,"cpuPercent":0,"dropouts":0},"midi":{"devices":[],"activity":{}}}}
```

`midiLearn` is advertised by the exact capability name `midiLearn` only when implemented. Each command returns one terminal reply with `result:{armed}` and leaves graph generation unchanged. Arming/re-arming clears the prior observation and resets a **30-second engine-side no-capture timeout**. Disarm, engine restart or a new session clears Learn; a captured CC remains visible until disarm/re-arm. No setting, patch or module config stores this state. The UI alone commits the observed controller through `ccMap.set` / `addModule` and its own lossless D8 `config.rows`.

A `swap` command acknowledges only **after** the candidate graph successfully validates, the old graph is safely retired and the new graph becomes active; refusal leaves the existing graph and generation untouched. Builds and MIDI subscription refreshes run on the control thread; source removal releases its held notes, and module IDs are not recycled within a session (D7). Live `setParam` and keyboard events acknowledge after successful queue insertion; on a full queue respond `busy`, not success. No callback performs JSON work. Graph switch gap ≤150 ms applies to normal patch edits, not hardware unplug/recovery. Refusals do not mutate audio, graph, device settings or the UI's editable document.

## 5. Execution view: document vs runnable graph (D3, D9)

UI `PatchV2` is the authority for saving: `format:"wirescape.native.patch"`, root `version:2`, `modules`, `connections`, optional `ui`, and arbitrary unknown fields. Each module has stable numeric `id`, stable string `module`, `parameters` and unknown fields. The v2 runtime additionally requires root `settings`, positive per-module `version`, and nonempty `outputs` entries `{module,port,bus_channel}`.

- `loadGraph` makes a **deep execution copy**. If `settings` is absent, use `{}` **only in that copy**. If a module's `version` is absent, fill the positive version from its native descriptor **only in that copy**; do not overwrite an explicitly supplied version. Existing `config`, `state`, unknown keys, order and exact saved document bytes belong to the UI.
- If `outputs` is **absent**, use the lowest-numbered `essentials.output` module with an `audio` output port; in the execution copy route its `audio` port to `bus_channel` 0 and 1 (the present native mono-to-stereo output convention). If no valid Output module exists, refuse `refused`. If `outputs` exists, validate it verbatim; do not replace an empty/invalid array or silently heal dangling routes.
- Refuse atomically: unknown/inactive modules (`unknown-module`/`unavailable`), unknown parameters/ports (`not-found`), invalid event-to-signal cables (`kind-mismatch`), duplicate IDs/edges (`duplicate`), >64 modules, >128 connections, patch/wire >2 MiB or nesting >64 (`limit`/`invalid` as appropriate), out-of-range parameters, or invalid output/config/state (`refused`/`invalid`). Preserve every unsupported object/field in the **UI** document; show its module as “not available” rather than removing it. Cable hints (audio/pitch/CV) never block connections; only signal/event kind and descriptor validity do.
- **Module ID namespace (Keel ruling, 2026-11-06, refines D7):** module IDs are the document's saved IDs and are never rewritten. "Never reused" applies to incremental edits within the currently loaded document: after a `removeModule`, that ID is not accepted by a later `addModule` until the next successful `loadGraph`. A successful `loadGraph` starts a fresh ID namespace (new `gen`), so switching between patches that use the same IDs for different modules (e.g. the three `class_demo_*` patches) is valid. Before the swap the engine drains MIDI/control work bound to the old graph and releases its held notes; replies and telemetry carry the new `gen`, and anything tagged with an older `gen` is ignored by the UI.
- Incremental edit validation is against the currently committed graph and native catalogue. Parameter modulation is normalized, additive, clamped and tapered in the engine, with the same stable parameter IDs. `setModAmount` is a graph swap, **not** a realtime knob event.

## 6. Device catalogue, MIDI endpoint selection and persistence

`Device` matches UI `EngineClient.Device`: `{id:string,name:string,kind:"audio"|"midi",enabled?:boolean,sampleRates?:number[]}`, with the additional MIDI `system_name` and optional audio `index` (informational only). `listAudioDevices` lists PortAudio devices with explicit output support metadata; the persistent `id` is `"<host API>|<name>"`, with `#<n>` appended for the n-th (n ≥ 2) output sharing that host API and name in enumeration order (Windows lists each endpoint once per host API and repeats names inside one, e.g. `"Windows WDM-KS|Headphones ()#2"`), never a PortAudio index, which shifts on hot-plug. Audio devices also carry optional `hostApi` (display only). A stored pre-identity bare name is upgraded once when exactly one output has it; when several do it can never open, so the default output is used instead (Keel, 2026-11-09). Only advertise actual supported sample rates; omit `sampleRates` when unknown. Optional, additive audio entry fields in `devices` and the initial `hello.audioDevices` are `backend:string` (exact PortAudio host-API name), `outputChannels:integer` (`maxOutputChannels`), optional `defaultSampleRate:number` (PortAudio-reported default), `supported:boolean`, and `unsupportedReason` **only if** `supported:false`, restricted to `"mono-output"`, `"no-output"` or `"rate-unsupported"`. The list includes mono and no-output entries, but disables them before open: v0 renders stereo, and **no** mono downmix is implemented. Only advertise confirmed sample rates; omit `sampleRates` when unknown.

A backend invalid-sample-rate failure, whether discovered before open or at stream open/start, returns `refused` with message `rate-unsupported`; `status.audio.reason` names `rate-unsupported` while the previous route keeps running. If startup has no previous stream, audio remains `no-device` with that reason until an explicit supported configuration succeeds. It never enters a retry/reconnect loop for a rate refusal.

An unknown/stale `deviceId` refuses `not-found`, two current devices with the same `id` refuse `duplicate`, and an unsupported device refuses `refused` with its reason. All three refusals occur **before** stream shutdown: the previously active route keeps playing without substitution. For device loss and reconnection, continue emitting `device-lost`/`reconnecting`, never silently select a default. Example enumeration followed by unsupported-mono refusal (the stereo stream remains active):
```json
{"v":0,"id":"list1","type":"listAudioDevices"}
{"v":0,"session":"s1","re":"list1","ok":true,"gen":"3","result":{"devices":[{"id":"USB Stereo","name":"USB Stereo","kind":"audio","index":3,"backend":"ALSA","outputChannels":2,"supported":true,"sampleRates":[44100,48000]},{"id":"Mono DAC","name":"Mono DAC","kind":"audio","index":4,"backend":"ALSA","outputChannels":1,"supported":false,"unsupportedReason":"mono-output"}]}}
{"v":0,"id":"mono1","type":"selectAudioDevice","deviceId":"Mono DAC"}
{"v":0,"session":"s1","re":"mono1","ok":false,"code":"refused","message":"mono-output","gen":"3"}
```
A separate stale-id exchange returns `{"v":0,"session":"s1","re":"stale1","ok":false,"code":"not-found","message":"No such audio output","gen":"3"}` for `{"v":0,"id":"stale1","type":"selectAudioDevice","deviceId":"disconnected-old"}`, without changing the active route.

`listMidiDevices` lists **libremidi input** endpoints with exact `name` and `system_name` (as `play --list-midi` prints). Stable UI id is `name + "|" + system_name` (never the volatile backend index), `enabled` reflects actual remembered selection and authorization. The `midi.in` instance `config` selects either:
- `{}` or `{"endpoint":"all_enabled"}` = **all currently enabled** MIDI inputs; enabling another device includes it automatically. Both spellings are equivalent (`native/control/midi_host.cpp`); the UI writes back whichever form the patch already had and never normalises one into the other.
- `{"endpoint":"M8 MIDI 1","system_name":"M8:M8 MIDI 1"}` = exactly that input, only while enabled. A missing or disabled named endpoint produces no events for this node, never falls back to all. No `endpoint:"*"` or numeric index in saved patches.

Engine persists selected audio `id`, the last successfully applied `audio.sampleRate`/`audio.bufferFrames` settings, enabled MIDI `(name,system_name)` pairs and explicit device preferences to `user_data/settings.json` (or `--settings <file>`), via atomic replace on its control side; the **patch document** never stores these app-level choices. `--device` overrides the chosen audio output for that invocation; a successful selection becomes the remembered preference. On hotplug/loss/reconnection, emit `devices` immediately (not throttled), update `status.audio.state` (`starting`, `running`, `no-device`, `device-lost`, `reconnecting`) and retry without fabricating a substitute device. MIDI subscriptions are refreshed on the control side, and held notes on lost/removed sources are released. When `--confirm-audio`/`--confirm-midi` were not passed, neither subsystem opens a real device (listing is permitted without opening streams, subject to the backend's enumeration safety).

## 7. `hello`, `status` and telemetry: actual observations only

`hello` is the **first stdout object** after startup, before any reply: `{"v":0,"session":"...","type":"hello","protocol":0,"engine":{"version":"...","sha":"..."},"capabilities":[...],"catalogue":{"modules":[...]},"audio":{...},"midi":{...}}`. `catalogue` uses the **UI `public/wirescape_modules.json` shape**: each module has `id,name,category,ports,parameters`; ports have `id,name,direction,kind,hint,poly,stereo_pair`; parameters have `id,name,minimum,maximum,default,unit,taper,step,decimals,modulatable` and optional `choices`. Only actually loadable modules appear (D9); native manifests generate the distributable JSON and CI checks drift. Capabilities list optional implemented methods by their exact wire names and contains `["midiLearn","telemetry.playhead","setSampleRate","setBufferFrames","osc"]` when the sidecar OSC host is installed; **never** advertise `setOsc` in Phase 1. `audio` and `midi` are the same structures as the status payload below.

`devices` is an unthrottled push `{v:0,session,type:"devices",gen,audioDevices:Device[],midiDevices:Device[]}` after any enumeration/selection/hotplug change. The list replies use exactly the corresponding arrays. The first `hello` carries current device state; `devices` need not repeat it unless something changes.

`status` at **at most 10 Hz**, coalescing discrete state transitions into the next sample, has `{v:0,session,type:"status",gen,status:SystemStatus}`. `SystemStatus` keys match `EngineClient.ts` exactly:
- `audio`: `{state,deviceId,deviceName,sampleRate,bufferFrames,latencyMs,cpuPercent,dropouts}`. `deviceId`/`deviceName` are `null` without an active output. `sampleRate` and `bufferFrames` are configured actual values; `latencyMs` is the UI's **calculated round-trip estimate** `2*bufferFrames/sampleRate*1000` (not a measured driver round trip). `cpuPercent` is measured callback wall-time / block duration ×100; `dropouts` is the real cumulative xrun/dropout count. When no callback is running, CPU is 0 (no callback work) and the UI displays **“not available”** for CPU/latency instead of interpreting that as measured device performance. Never invent a device, CPU sample or dropout.
- `midi`: `{devices:Device[],activity:{[deviceId]:number},learn?}`, with existing 0..1 activity unchanged. `learn` is **omitted when unarmed**, `{armed:true}` before the first CC, then `{armed:true,seq:"<decimal u32>",deviceId:string,channel:1..16,controller:0..127,value:0..127}` while armed. Clients compare wrapping `seq` **for inequality only**. Exactly one latest-wins observation is held, not a queue/history; removed/unknown endpoint slots cannot be mapped to a different device and are dropped. The MIDI callback performs only bounded O(1) lock-free work: guarded by atomic armed, stores an endpoint slot (8 bits), channel (4), controller (7), value (7) and sequence (32) in one packed lock-free `atomic<uint64_t>`; no allocation, locks, I/O, logging or JSON. Only the control thread handles timeout, slot-to-device mapping and status serialization. No CC14 pairing, NRPN or RPN parsing belongs to Learn; 14-bit row configuration remains explicitly UI-owned. Omit optional `clock` unless observed and running.


- `osc` (additive when `hello.capabilities` contains `osc`): `{enabled:boolean,bind:"127.0.0.1"|"0.0.0.0",port:integer,lan:boolean,last_error?:string,routes?:OscRoute[]}`. `enabled` means at least one UDP listener is bound, not that the saved patch has a source. `bind` is the broadest **actual** binding (wildcard only if an authorized LAN listener is running), `lan` reflects that actual LAN binding and `port` is the first configured route port (0 with no routes); the bounded `routes` list disambiguates multi-port patches. Each `OscRoute` has `{module:string,port:integer,bind:string,state:"listening"|"lan-not-confirmed"|"port-busy",received:integer,dropped:integer}`, at most 32 entries and no datagram payload. The safe optional `last_error` is `"port-busy"` when any requested port is unavailable. A busy port does **not** refuse the patch and cannot silently enable network access.
- OSC has one control-owned nonblocking UDP worker with a fixed 65,507-byte receive buffer, maximum accepted datagram 2,048 bytes, and at most 64 packets processed per socket per poll iteration. Each physical UDP port is shared between matching module routes; a conflicting loopback/LAN permission on the same port cannot broaden another route's bind. The worker records the actual monotonic receive timestamp and a committed graph-generation tag on enqueue; it stops and joins before graph retirement, replan or audio teardown. Only the control worker parses OSC and maintains bounded counters. The render callback never opens sockets, parses, allocates, locks or logs OSC. Optional/unknown status fields are ignored by readers.

`telemetry` at **at most 30 Hz**, latest-sample-wins when the receiver is slow, has `{v:0,session,type:"telemetry",gen,telemetry:Telemetry}`. Its required `Telemetry` keys match `EngineClient.ts`: `tick` (monotonic observation counter), `time` (actual seconds since engine startup), `effective:{}`, `phase:{}`, `envelope:{}`, `meters:{}`, `scope:{}`, `eventActivity:{}`, `heldNotes:[]`. Optional `outputs` (`"id:port"` → last sample, channel 0, of a cabled signal output) is ratified: web emits it; sidecar may omit it. For v0:
- `meters` contains **only observed** `essentials.output` module IDs, keyed as JSON decimal object keys, with measured `[left,right]` linear levels 0..1+ (clipping >1). No generic mixer/other-meter simulation. Missing module meter entry means **“not available”**.
- `heldNotes` is an accurate snapshot of note numbers still held (MIDI ingress and renderer keyboard), not a synthetic piano demo.
- `effective[id][param]` is the actual last-frame rendered post-modulation parameter value in plain units, present **only** for parameters with modulation cables (including amount zero). `envelope[id]` is the measured last-frame `envelope` output of ADSR or Function Envelope, reduced by maximum across voices and clamped to 0..1. `phase[id]` is the measured channel-0 0..1 `phase` output of an LFO when the B-wire capability is installed; until then it is `{}`. `eventActivity["id:port"]` is a pulse for an event output whose cumulative count advanced since the previous observed frame (`1` on advancement, else the previous activity ×0.6, omitted below 0.05). Counts and cursors reset on **every successful graph commit**, including live replan and D5. A burst between reads produces a single pulse, not one flash per event.
- These four maps are **complete snapshots**, not deltas or tombstones. Clients replace them on every telemetry message; absent means unavailable/no activity. When no render frame advanced since the last sample (stopped, parked or lost), all four are `{}`. The outer `gen` identifies the graph that installed the taps, and older generations are ignored by Studio. `scope` remains `{}` in the sidecar; if it is transmitted as JSON number arrays, `RealEngine` converts each to `Float32Array`. No value is guessed or interpolated.
- Realtime produces only preallocated bounded atomics/snapshots. Non-RT sampler packages JSON; no callback serialization, heap allocation or unbounded copying. Never let slow IPC block the render thread. Missing/unsupported metrics are absent or empty as above; the adapter may not replace them with plausible demo numbers.

When `hello.capabilities` advertises **`telemetry.playhead`**, optional
`playhead:{"<Groove moduleId>":[t1,t2]}` and
`groove:{"<Groove moduleId>":[{step,loop,playing,fired},{step,loop,playing,fired}]}`
are installed together. Both maps come from the **same render-owned last-frame
snapshot**, including uncabled Groove observation outputs. `step` (also the
corresponding `playhead` value) is an integer 0..63 while active, or **-1 only
when the global transport and the track have stopped, its gate is zero and all owed NOTE_OFFs have drained** (a disarmed track under a running transport reports the transport step);
`playing` is exactly `step >= 0`. Mute or a queued STOP/ACK does not prove STOP.
`loop` is the exact uint32 per-track loop count (starts 1 on Start/Reset);
the native sampler reconstructs the low/high 16-bit signal halves off-render.
`fired` is a boolean condition result for the current note trig, inherited by
continued ties, false for rests/failed conditions/stopped tracks. Before G1
conditions every existing note trig passes. These complete maps reset on every
successful commit, carry the outer `gen`, and are `{}` whenever no new render
frame advanced. No older or absent entry authorizes Apply. Studio enables
stopped Apply only for current-generation `playhead[id] == [-1,-1]`, or when
current-generation audio status is not `running`. The capability is omitted if
this observation seam is unavailable; no timing interpolation is permitted.

§7 additive telemetry example (same envelope as T13):
```json
{"v":0,"session":"s1","type":"telemetry","gen":"5","telemetry":{"tick":12,"time":0.4,"effective":{},"phase":{},"envelope":{},"meters":{},"scope":{},"eventActivity":{},"heldNotes":[],"playhead":{"1":[3,-1]},"groove":{"1":[{"step":3,"loop":16777217,"playing":true,"fired":false},{"step":-1,"loop":1,"playing":false,"fired":false}]}}}
```

## 8. `EngineClient` refusal, ordering and reconciliation

`FakeEngine` keeps its existing synchronous API and remains the browser-test backend. `RealEngine` implements the unchanged `void` edit methods by queuing a command and may additionally expose **optional** `onRefusal?(cb: (r: {requestId:string; type:string; detail:EngineError; gen:string}) => void): () => void` on `EngineClient`. This is additive: code must feature-detect `onRefusal` and not require `FakeEngine` to implement it. `loadGraph` and device-list methods already return Promises; their errors reject with `EngineRefusal`.

For every optimistic UI edit, `RealEngine` records `{id,session,mutation,inverse}` in an ordered pending journal before sending. The UI retains a last-acknowledged executable projection separately from its original save document. On success, promote that edit in order; on refusal, emit `onRefusal`, remove the failed edit, restore the last confirmed projection and **replay/revalidate** later pending mutations in order, rolling back dependents that can no longer apply. The visible editable document rolls back **only the refused edit and its invalid dependents**, not unknown fields or unrelated saved metadata. Show the refusal code/message once. Never infer success from timeout or optimistic animation.

Because control processing is serialized, `gen` tracks committed topology and can reconcile ordered replies. A stale `gen` from an old process/session, late reply after timeout, or message for an unknown request ID is ignored; any uncertain pending sequence (child exit, IPC failure, lost acknowledgment) triggers a full `loadGraph` from the UI's current document after the next `hello` rather than guessing. No edit is silently retried against a different graph generation. On `loadGraph` refusal keep the document editable and surface unavailable modules/cables; the running graph remains unchanged. UI must await/catch `loadGraph` (including on initial file open).

## 9. Startup flags and device-off testing

`wirescape-engine [--confirm-audio] [--confirm-midi] [--confirm-lan] [--device NAME] [--block FRAMES] [--rate HZ] [--settings FILE] [--null-audio]`. `--confirm-*` are explicit consent from the owner's app launcher (Electron forwards them verbatim); **without** them, startup does not open real audio/MIDI devices. OSC binds `127.0.0.1` by default; binding `0.0.0.0` additionally requires both owner-supplied `--confirm-lan` (forwarded verbatim by Electron only when explicitly requested at app launch) **and** `lan:true` on that OSC In route. The app must not enable LAN on behalf of the user. Unconfirmed `lan:true` falls back to loopback, not a patch refusal. A busy port leaves the patch loaded and the route inactive. `--device` selects an exact PortAudio output name; `--block` is 1..4096 frames, default 256; `--rate` is a positive supported Hz, default 48000; `--settings FILE` overrides the default user_data/settings.json for isolated tests. Unknown/missing/invalid flags exit nonzero with stderr diagnostics, no device opened. `--null-audio` is the **test path**: a steady-clock-paced render into a discard sink, with fake MIDI injection, no PortAudio stream, no libremidi device open, even if `--confirm-*` are also supplied; it produces real rendered meters/counters, not fabricated signals. CI uses `--null-audio --settings <temp>` and no confirmation flags. Only the consent-gated OSC In UDP sidecar worker opens a network socket; tests use loopback only, never real LAN or physical devices.

## 10. Normative example exchanges (P1B + P1C fixtures)

Each line below is a standalone JSON wire line. `>` denotes renderer→engine stdin, `<` denotes engine stdout; `@` denotes a main-process-owned IPC message (not sent to child). Use a fresh `s1` session with `gen:"0"` unless the example shows another state; fixture catalogue contains only Output and VCO, while T07/T12 have their own stated setup. IDs and devices in examples are illustrative. Fixtures must also assert **no other stdout text**, exact reply ID matching, a bounded line, and unchanged generation on refusal.

**T01 — first `hello`, catalogue and capabilities.**
```text
< {"v":0,"session":"s1","type":"hello","protocol":0,"engine":{"version":"0.1","sha":"abc1234"},"capabilities":["midiLearn","telemetry.playhead","setSampleRate","setBufferFrames","osc"],"catalogue":{"modules":[{"id":"essentials.output","name":"Output","category":"I/O","ports":[{"id":"audio","name":"Audio","direction":"input","kind":"signal","hint":"audio","poly":"sum","stereo_pair":null},{"id":"audio","name":"Audio","direction":"output","kind":"signal","hint":"audio","poly":"poly","stereo_pair":null}],"parameters":[]},{"id":"essentials.vco","name":"VCO","category":"Oscillator","ports":[{"id":"pitch","name":"Pitch","direction":"input","kind":"signal","hint":"pitch","poly":"poly","stereo_pair":null},{"id":"fm","name":"FM Hz","direction":"input","kind":"signal","hint":"audio","poly":"poly","stereo_pair":null},{"id":"notes","name":"Notes","direction":"input","kind":"event","hint":"ev_notes","poly":"poly","stereo_pair":null},{"id":"audio","name":"Audio","direction":"output","kind":"signal","hint":"audio","poly":"poly","stereo_pair":null}],"parameters":[{"id":"frequency","name":"Frequency","minimum":0.01,"maximum":20000,"default":220,"unit":"Hz","taper":"linear","step":0,"decimals":3,"modulatable":true}]}]},"audio":{"state":"no-device","deviceId":null,"deviceName":null,"sampleRate":48000,"bufferFrames":256,"latencyMs":10.6667,"cpuPercent":0,"dropouts":0},"midi":{"devices":[],"activity":{}}}
```

**T02 — load a minimal execution view; missing settings, per-module version and outputs are filled ONLY in engine copy.**
```text
> {"v":0,"id":"r2","type":"loadGraph","patch":{"format":"wirescape.native.patch","version":2,"modules":[{"id":7,"module":"essentials.output"}],"connections":[],"ui":{"labels":{"7":"Out"}}}}
< {"v":0,"session":"s1","re":"r2","ok":true,"gen":"1"}
```

**T03 — unavailable module is refused without rewriting the editable patch or advancing the generation.**
```text
> {"v":0,"id":"r3","type":"addModule","moduleId":8,"moduleType":"absent.vco"}
< {"v":0,"session":"s1","re":"r3","ok":false,"code":"unknown-module","message":"Module not installed","gen":"1"}
```

**T04 — add a known node, then connect a valid signal edge as two separate graph swaps.**
```text
> {"v":0,"id":"r4a","type":"addModule","moduleId":9,"moduleType":"essentials.vco","params":{"frequency":220}}
< {"v":0,"session":"s1","re":"r4a","ok":true,"gen":"2"}
> {"v":0,"id":"r4b","type":"connect","connection":{"kind":"signal","from":{"module":9,"port":"audio"},"to":{"module":7,"port":"audio"}}}
< {"v":0,"session":"s1","re":"r4b","ok":true,"gen":"3"}
```

**T05 — invalid cable kind; no partial graph commit.** (Precondition: source and destination descriptor kinds mismatch.)
```text
> {"v":0,"id":"r5","type":"connect","connection":{"kind":"event","from":{"module":9,"port":"audio"},"to":{"module":7,"port":"audio"}}}
< {"v":0,"session":"s1","re":"r5","ok":false,"code":"kind-mismatch","message":"Event cable requires event ports","gen":"3"}
```

**T06 — live parameter event: accepted for queueing, generation stays put.**
```text
> {"v":0,"id":"r6","type":"setParam","moduleId":9,"paramId":"frequency","value":440}
< {"v":0,"session":"s1","re":"r6","ok":true,"gen":"3"}
```

**T07 — modulation amount change uses D5 swap (precondition: the named modulation cable exists).**
```text
> {"v":0,"id":"r7","type":"setModAmount","connection":{"kind":"modulation","from":{"module":10,"port":"control"},"to":{"module":9,"port":"frequency"},"amount":0.2},"amount":-0.5}
< {"v":0,"session":"s1","re":"r7","ok":true,"gen":"4"}
```

**T08 — keyboard NoteOn/NoteOff and priority panic do not advance graph generation.**
```text
> {"v":0,"id":"r8a","type":"noteOn","note":60,"velocity":0.8}
< {"v":0,"session":"s1","re":"r8a","ok":true,"gen":"4"}
> {"v":0,"id":"r8b","type":"noteOff","note":60}
< {"v":0,"session":"s1","re":"r8b","ok":true,"gen":"4"}
> {"v":0,"id":"r8c","type":"panic"}
< {"v":0,"session":"s1","re":"r8c","ok":true,"gen":"4"}
```

**T09 — PortAudio stereo outputs: name is the persistent id, index is only informational.**
```text
> {"v":0,"id":"r9","type":"listAudioDevices"}
< {"v":0,"session":"s1","re":"r9","ok":true,"gen":"4","result":{"devices":[{"id":"USB Stereo","name":"USB Stereo","kind":"audio","index":3}]}}
> {"v":0,"id":"r9b","type":"selectAudioDevice","deviceId":"USB Stereo"}
< {"v":0,"session":"s1","re":"r9b","ok":true,"gen":"4"}
```

**T10 — MIDI endpoints carry `name` and `system_name`, and changing enabled state is control-side.**
```text
> {"v":0,"id":"r10a","type":"listMidiDevices"}
< {"v":0,"session":"s1","re":"r10a","ok":true,"gen":"4","result":{"devices":[{"id":"M8 MIDI 1|M8:M8 MIDI 1","name":"M8 MIDI 1","system_name":"M8:M8 MIDI 1","kind":"midi","enabled":true}]}}
> {"v":0,"id":"r10b","type":"setMidiEnabled","deviceId":"M8 MIDI 1|M8:M8 MIDI 1","on":false}
< {"v":0,"session":"s1","re":"r10b","ok":true,"gen":"4"}
```

**T11 — device notification is unthrottled and tied to real enumeration.**
```text
< {"v":0,"session":"s1","type":"devices","gen":"4","audioDevices":[{"id":"USB Stereo","name":"USB Stereo","kind":"audio","index":3}],"midiDevices":[{"id":"M8 MIDI 1|M8:M8 MIDI 1","name":"M8 MIDI 1","system_name":"M8:M8 MIDI 1","kind":"midi","enabled":false}]}
```

**T12 — CC row write is a graph swap (precondition: node 12 is `midi.cc_to_cv`).**
```text
> {"v":0,"id":"r12","type":"ccMap.set","moduleId":12,"row":0,"mapping":{"channel":0,"controller":21,"fourteen_bit":false,"bipolar":false,"smoothing_ms":10}}
< {"v":0,"session":"s1","re":"r12","ok":true,"gen":"5"}
```

**T13 — status and measured-only telemetry (empty unsupported fields, never fake scope).**
```text
< {"v":0,"session":"s1","type":"status","gen":"5","status":{"audio":{"state":"running","deviceId":"USB Stereo","deviceName":"USB Stereo","sampleRate":48000,"bufferFrames":256,"latencyMs":10.6667,"cpuPercent":2.1,"dropouts":0},"midi":{"devices":[],"activity":{}},"osc":{"enabled":false,"bind":"127.0.0.1","port":0,"lan":false,"routes":[]}}}
< {"v":0,"session":"s1","type":"telemetry","gen":"5","telemetry":{"tick":12,"time":0.4,"effective":{},"phase":{},"envelope":{},"meters":{"7":[0.1,0.1]},"scope":{},"eventActivity":{},"heldNotes":[]}}
```

Device values in T09/T13 illustrate a real device; `--null-audio` reports `no-device` with null ids.

**T14 — malformed JSON and overload never kill the process or mutate the graph.**
```text
> {"v":0,"id":"r14",BROKEN
< {"v":0,"session":"s1","re":null,"ok":false,"code":"invalid","message":"Malformed JSON","gen":"5"}
> {"v":0,"id":"r14b","type":"setParam","moduleId":9,"paramId":"frequency","value":440}
< {"v":0,"session":"s1","re":"r14b","ok":false,"code":"busy","message":"Command queue full","gen":"5"}
```

**T15 — main-owned restart, exit, new session and UI resend; engine stdin EOF exits 0.**
```text
@ {"v":0,"id":"a15","type":"app.restartEngine"}
@ {"v":0,"re":"a15","ok":true,"gen":"0"}
@ {"v":0,"type":"app.engineExited","exitCode":0,"signal":null,"expected":true}
< {"v":0,"session":"s2","type":"hello","protocol":0,"engine":{"version":"0.1","sha":"abc1234"},"capabilities":["midiLearn","telemetry.playhead","setSampleRate","setBufferFrames","osc"],"catalogue":{"modules":[{"id":"essentials.output","name":"Output","category":"I/O","ports":[{"id":"audio","name":"Audio","direction":"input","kind":"signal","hint":"audio","poly":"sum","stereo_pair":null},{"id":"audio","name":"Audio","direction":"output","kind":"signal","hint":"audio","poly":"poly","stereo_pair":null}],"parameters":[]},{"id":"essentials.vco","name":"VCO","category":"Oscillator","ports":[{"id":"pitch","name":"Pitch","direction":"input","kind":"signal","hint":"pitch","poly":"poly","stereo_pair":null},{"id":"fm","name":"FM Hz","direction":"input","kind":"signal","hint":"audio","poly":"poly","stereo_pair":null},{"id":"notes","name":"Notes","direction":"input","kind":"event","hint":"ev_notes","poly":"poly","stereo_pair":null},{"id":"audio","name":"Audio","direction":"output","kind":"signal","hint":"audio","poly":"poly","stereo_pair":null}],"parameters":[{"id":"frequency","name":"Frequency","minimum":0.01,"maximum":20000,"default":220,"unit":"Hz","taper":"linear","step":0,"decimals":3,"modulatable":true}]}]},"audio":{"state":"no-device","deviceId":null,"deviceName":null,"sampleRate":48000,"bufferFrames":256,"latencyMs":10.6667,"cpuPercent":0,"dropouts":0},"midi":{"devices":[],"activity":{}}}
> {"v":0,"id":"r15","type":"loadGraph","patch":{"format":"wirescape.native.patch","version":2,"modules":[{"id":7,"module":"essentials.output"}],"connections":[]}}
< {"v":0,"session":"s2","re":"r15","ok":true,"gen":"1"}
```

**T16 — additive MIDI Learn command/status (tested separately with fake-MIDI input and null audio; no fake-injection wire command).**
```text
> {"v":0,"id":"r16a","type":"midiLearn","armed":true}
< {"v":0,"session":"s1","re":"r16a","ok":true,"gen":"5","result":{"armed":true}}
< {"v":0,"session":"s1","type":"status","gen":"5","status":{"audio":{"state":"no-device","deviceId":null,"deviceName":null,"sampleRate":48000,"bufferFrames":256,"latencyMs":10.6667,"cpuPercent":0,"dropouts":0},"midi":{"devices":[],"activity":{},"learn":{"armed":true}}}}
# Fake MIDI harness delivers channel-2 CC74 value110 on the known endpoint M8 MIDI 1|M8:M8 MIDI 1.
< {"v":0,"session":"s1","type":"status","gen":"5","status":{"audio":{"state":"no-device","deviceId":null,"deviceName":null,"sampleRate":48000,"bufferFrames":256,"latencyMs":10.6667,"cpuPercent":0,"dropouts":0},"midi":{"devices":[],"activity":{},"learn":{"armed":true,"seq":"1","deviceId":"M8 MIDI 1|M8:M8 MIDI 1","channel":2,"controller":74,"value":110}}}}
> {"v":0,"id":"r16b","type":"midiLearn","armed":false}
< {"v":0,"session":"s1","re":"r16b","ok":true,"gen":"5","result":{"armed":false}}
```

**Fixture invariants:** Strictly parse **every valid JSON line** in T01–T15 with duplicate-key detection (including nested objects), rejecting the deliberately malformed input in T14. Verify in particular that T09 selection and T10b carry **both** the request `id` and distinct `deviceId`, and that the file bridge can open/save/cancel using stubbed dialogs with no path exposed. Test T01 before the first engine reply; T02 must leave UI `ui.labels` and original missing fields unchanged; T05/T14 keep gen unchanged; T06/T08 never increment gen; T07/T12 increment only on commit; T09/T10 use real/fake enumerators, never hardware; T13 `meters` reflects a rendered null-sink patch, not arbitrary fixed numbers; T15 ignores any late `s1` reply. Separately test an over-2-MiB line (`limit`/drain to LF), exactly 256 queued commands (257th `busy`), an unsupported optional `setOsc` (`unavailable`), and stdin EOF with audio/MIDI teardown and process exit code 0. All tests use a fake backend and `--null-audio`; no real device, LAN, or microphone is opened.
