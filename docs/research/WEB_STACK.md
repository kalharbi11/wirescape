<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Research: web shell and front-end stack for Wirescape

**Research date:** 2026-11-01. **Status:** planner proposal, not a stack approval or implementation. **Repository basis:** `main` at `6e3c253550428f2bb964af4434bd429b5284cce2`; protocol research in draft [#211](https://github.com/kalharbi11/wirescape/pull/211), branch `research/engine-ui-protocol`, head `b53ca8a18eb1712a2c576121f15b31b79a83b0ad`. **Platforms:** Windows first; Raspberry Pi 5 ARM64 is the performance target. **Confidence:** documentary platform claims high, comparative deployment predictions provisional, Pi GPU/physical touch unmeasured.

**Scope / one-file rule.** This research adds **only** `docs/research/WEB_STACK.md`. The full working `100-module / 400-cable` HTML benchmark is **embedded verbatim in §8** and is extractable to `docs/research/bench/web_renderer_100_400.html` using the documented command; a separately tracked bench file would violate the owner-authorized **ONE new file** limit. No source, device, LAN, PR review or merge changes. The owner sketch `WIRESCAPE Studio 01` was not attached; benchmark geometry is a synthetic approximation, not a claim of sketch parity.

## Executive recommendation

**Choose Tauri 2 for the Windows packaged shell, a standalone C++17 ABI-2 engine host as a supervised sidecar, and vanilla TypeScript-or-JavaScript modules with native SVG as the initial patch-canvas implementation.** Treat **Raspberry Pi OS Chromium kiosk** accessing the **same locally served UI** as the Pi baseline; do not force Tauri/WebKitGTK onto the Pi before accelerated-rendering and touch gates. Use Tauri IPC to bridge from a privileged Rust shell to the C++ host over framed local named pipes (Windows) / Unix domain sockets (Linux). Keep **one versioned JSON protocol** with an optional authenticated, loopback-only WebSocket adapter for a standalone kiosk. If Pi WebKitGTK passes measured gates, a Tauri ARM64 `.deb` can become a supported convenience package later. This recommendation deliberately does **not** select Electron, Qt WebEngine, React or PixiJS as v1 defaults.

**Important qualification:** the entire HTML/web shell is **research**, not currently in Wirescape. [ENGINE_PROTOCOL.md §4](https://github.com/kalharbi11/wirescape/blob/research/engine-ui-protocol/docs/research/ENGINE_PROTOCOL.md#L130-L153) proposes the contract; it does not implement it. Shell build size/idle RSS/cold-start were **not** measured in this environment. Only the local **Chromium renderer stress benchmark** was executed. A faster HTML-only benchmark is not proof of UI responsiveness while engine, update agent or audio/MIDI are active.

## 1. Existing-code facts and protocol boundary

| Observed fact (not a recommendation) | Primary code evidence |
|---|---|
| Native C++17 root has `WIRESCAPE_BUILD_UI` option, and Qt shell is added only if that option is on. | `native/CMakeLists.txt:1-5,17,99-101` |
| Qt-free native loader owns a **2 MiB** input limit, parses/loads but **does not write** patches; the preservation-capable editor/migration writer remains Qt-backed. | `native/document/runtime_patch.h:5-10`; `native/document/CMakeLists.txt:8-10,25-32` |
| Qt is optional for the document tooling: `Qt6::Core` is found conditionally; `wirescape_document` links it when available. | `native/document/CMakeLists.txt:20-32` |
| Current Studio controller rejects topology changes when running or callback-owned, while parameter changes may enter the live event queue. | `native/ui/studio_controller.cpp:298-316` |
| Existing controller meter update interval is **50 ms**, i.e. nominal **20 Hz**, not 60 Hz. | `native/ui/studio_controller.cpp:92-103` |
| Existing Qt-free PortAudio `play()` blocks in a polling loop and retains the engine on an unrecoverable stop error; it cannot directly serve a responsive `audio.stop` RPC in a single IPC dispatcher. | `native/io/audio/portaudio_service.cpp:70-91` |
| Engine ingress is separate from render; render calls the engine and updates atomics. | `native/control/app_engine.cpp:72-100` |
| The current bridge presents 24 properties, 23 invokables, 4 signals; conversion to a protocol is **proposed** in #211, not deployed. | `native/ui/studio_controller.h:19-42,65-114`; `docs/research/ENGINE_PROTOCOL.md@research/engine-ui-protocol:6-12,51-90` |

**Do not equate Qt-free playback with a Qt-free editable patch application.** The proposed host must port the authoritative v2 read/edit/migrate/atomic-write behavior; it must not use JS `JSON.stringify` over a UI projection as a save source. Stable node/port/parameter IDs, unknown fields, v1 backup bytes and unchanged v2 semantic data survive edits; the policy for unknown JSON numeric lexemes and object ordering still requires planner choice (`docs/research/ENGINE_PROTOCOL.md@research/engine-ui-protocol:112-129,225-227`). The wire protocol **v1** is distinct from patch **v2** and ABI **2**.

## 2. Shell platform and deployment comparison

**Reading rule:** *reported* numbers are from linked vendor/project documents, not Wirescape builds; *N/M* = not measured or not comparably published. A launcher binary, installer download, installed footprint and total resident set are distinct quantities. Never interpret the latter three from a binary file size.

| Shell | Windows renderer and installer | Pi 5 / Linux ARM64 path | Installed/runtime footprint, start and memory |
|---|---|---|---|
| **Tauri 2** | OS Edge **WebView2**; NSIS `.exe` or WiX MSI. Default installer downloads runtime if absent. WebView2 runtime is generally preinstalled on supported contemporary Windows. | GTK3 + **WebKitGTK 4.1**, not Chromium; `.deb` suitable for pinned Raspberry Pi OS, ARM64 AppImage must be built on ARM (not normally cross-compiled). | Windows vendor-reported *additional* WebView2 payload **0 MB** download-bootstrapper, **~1.8 MB** embedded bootstrapper, **~127 MB** offline installer, **~180 MB** fixed runtime (these are **not** whole-app installer sizes). Tauri docs state simple Linux package **2–6 MB** versus **70+ MB** AppImage in their example, not a Pi measurement. **Idle RSS N/M; cold-start N/M.** [Windows install](https://v2.tauri.app/distribute/windows-installer/), [AppImage](https://v2.tauri.app/distribute/appimage/), [prerequisites](https://v2.tauri.app/start/prerequisites/). |
| **Electron** | Bundles its **own Chromium + Node**, not WebView2; Electron Forge/Packager with Squirrel/NSIS/MSI tooling. | Official **linux-arm64** release target, typically X11 or Wayland/Ozone validation; extra bundled Chromium overhead may buy more consistent rendering. | **Reported illustrative update artifact** in the Electron docs: **103,298,365 bytes** for an Electron Fiddle *full `.nupkg`*; this is **not** a Wirescape installer or minimum footprint. **Idle RSS N/M; cold-start N/M.** Independent platform benchmark required. [Electron installation](https://www.electronjs.org/docs/latest/tutorial/installation), [updates](https://www.electronjs.org/docs/latest/tutorial/updates), [process model](https://www.electronjs.org/docs/latest/tutorial/process-model). |
| **`webview/webview` C++ library** | Direct Win32/Edge WebView2 embedding, no Rust/Tauri. DIY Windows MSIX/NSIS/WiX installer and WebView2 runtime detection. | GTK3/4 plus **WebKitGTK** on ARM64; depends on distro packages and compatible accelerated compositor. | Core bridge deliberately tiny, but end-to-end package/runtime RSS/cold-start **N/M** and dominated by platform webview. No comparable upstream packaged hello-world measurement located. [upstream README](https://github.com/webview/webview). |
| **Qt WebEngine** | Qt WebEngine uses embedded **Chromium/QtWebEngineProcess**, **not WebView2**. Deploy Qt DLLs/plugins/resources/process with `windeployqt`. | Native ARM64 Qt build + Chromium dependencies; embedded Qt app possible, but Chromium build/graphics integration materially expands the platform burden. | **Installer MB / idle RSS / cold-start N/M** for a same-version hello world. Runtime is multi-process; external Chromium resources must ship. [Qt deployment](https://doc.qt.io/qt-6/qtwebengine-deploying.html), [hardware acceleration](https://doc.qt.io/qt-6.10/qtwebengine-features.html). |

Tauri's Linux webview depends on the distribution's WebKitGTK version and rendering stack, whereas Electron/Qt WebEngine ship their Chromium stack. A Pi Chromium **kiosk is not a Tauri Linux renderer**; it is a separate deployment mode for the same web front end. Chromium kiosk can avoid bundling another webview but its **system package** is still a substantial dependency. Raspberry Pi officially documents Wayland/**labwc** and Chromium `--kiosk` ([official kiosk tutorial](https://www.raspberrypi.com/tutorials/how-to-use-a-raspberry-pi-in-kiosk-mode/); [OS configuration](https://www.raspberrypi.com/documentation/computers/configuration.html)).

### IPC, updates, signing, licences and CI surface

| Criterion | Tauri 2 | Electron | C++ `webview` | Qt WebEngine |
|---|---|---|---|---|
| JS → native | Tauri commands `invoke` and events; Rust adapter to C++ via named pipe/UDS (or signed sidecar launcher). `externalBin` requires a binary for each target triple. | Secure `contextBridge`/`ipcRenderer` → Node main → child-process/stdin pipes or FFI; maintain context isolation, sandbox and explicit allowed commands. | JS-to-C++ `bind` and C++-to-JS evaluation on UI thread; custom framed sidecar bridge or in-process C++ host. | `QWebChannel`/QObject bridge for in-process Qt, or custom socket/IPC to Qt-free host. |
| Engine in-process? | Possible via Rust/C FFI but crash, unwind and shared-lifetime hazards; not recommended. | Native Node addon/FFI possible, high ABI/build/rebuild complexity; not recommended. | Trivial C++ pointer integration; engine/plugin crash kills shell, weak fault isolation. | Native C++ integration possible; QtWebEngineProcess isolates renderer, **not** DSP native crash. |
| Auto-update | Official updater plugin for signed releases, dynamic/static manifest, Linux AppImage; updater's own signature is **mandatory** and distinct from Windows Authenticode. | Built-in `autoUpdater` on Windows/macOS with supported formats; **no built-in Linux updater**, use package manager. | None built-in; own updater/package policy. | None built-in for arbitrary Qt app; own updater/package policy. |
| Code signing | Windows Authenticode or Azure Artifact Signing for shipped `.exe`/installer and sidecar; Tauri update-signing private key securely held outside CI logs. | Sign `.exe` and installers via Forge/Builder; signing credentials and helper binaries handled in CI. | Sign host and installer yourself; ship Microsoft runtime redistributables as required. | Sign app/QtWebEngineProcess and installer; ensure all redistributed Qt/Chromium components. |
| OSS licences | Tauri core MIT or Apache-2.0; inspect Rust crates and platform WebKit/WebView2 redistributables. | Electron MIT; Chromium/Node and other bundled third-party notices need inventory. | `webview` **MIT**; system GTK/WebKitGTK licences remain. | Qt modules use LGPLv3/GPLv3/commercial variants, plus third-party Chromium notices; check each exact module/build and distribution obligations. |
| CI additions over existing C++17/CMake | **Rust/Cargo** (and updater plugin), Node/npm only for JS bundling, platform WebKitGTK dev packages, Win MSVC/WebView2, NSIS/WiX; C++ host build + per-arch externalBin staging + signing. | **Node/npm** + Electron binary downloads/cache, Forge packaging and codesign, C++ host cross-platform build. | C++17/CMake + WebView2 SDK/NuGet and Linux GTK/WebKitGTK dev headers + packaging/signing; little JS tooling if vanilla. | Qt6 Core/WebEngine + Chromium Qt build/deployment tooling and platform graphics plugins; potentially largest native CI setup. |

Evidence: [Tauri sidecars](https://v2.tauri.app/develop/sidecar/), [Tauri updater/signatures](https://v2.tauri.app/plugin/updater/), [Tauri licence](https://github.com/tauri-apps/tauri), [Electron security](https://www.electronjs.org/docs/latest/tutorial/security), [Electron autoUpdater](https://www.electronjs.org/docs/latest/api/auto-updater), [Electron signing](https://www.electronjs.org/docs/latest/tutorial/code-signing), [`webview` requirements/IPC/licence](https://github.com/webview/webview), [Qt WebEngine deployment/process](https://doc.qt.io/qt-6/qtwebengine-deploying.html), [Qt licensing](https://doc.qt.io/qt-6/licensing.html). The Tauri updater documents a current **Rust ≥1.90** plugin requirement; pin that and verify exact versions when establishing CI ([updater docs](https://v2.tauri.app/plugin/updater/)).

**Recommendation under uncertainty:** Tauri offers privileged, packaged Windows entry points without bundling Chromium, at the cost of a new Rust toolchain and two very different Windows/Pi renderer implementations. `webview` minimizes native glue and may be a contingency when Rust CI is prohibited, but delegates updater/signing/security/chrome integration to Wirescape. Electron provides consistent Chromium and tooling, with heavier payload and Node runtime. Qt WebEngine retains Qt rather than removing it and introduces its own Chromium. **Do not choose by the smallest third-party demo binary alone.**

## 3. Attempted hello-world shell builds and measurement validity

**Local host:** isolated Linux x86_64 container, **5,947 MiB** reported physical memory, Node **v22.16.0**, npm **10.9.2**, available `/usr/bin/chromium` **144.0.7559.96**; no physical Pi, Windows environment or writable repo checkout. Attempts, 2026-11-01:

1. Prepared a 2-file Electron `package.json`/`main.js` hello world in a temporary directory and ran `npm install --offline --no-audit --no-fund electron@44.0.0`. **FAILED ENOTCACHED**; registry DNS unavailable. No Electron binary/installer was made. Directory **12 KiB** is *source stub only*, **not installed size**.
2. Prepared the Tauri feasibility check: `rustc`, `cargo`, `cargo-tauri` **not installed**; WebKitGTK (`pkg-config webkit2gtk-4.1`) **not installed** and external downloads unavailable. **No Tauri build.** A fake substitute executable would not be evidence.
3. Consequently **no shell installer size, idle process-tree RSS or cold start is reported as measured**. Also no Windows MSI/NSIS build, signing experiment or ARM64 native benchmark. The upstream [Tauri maintained benchmark suite](https://tauri-apps.github.io/benchmark_results/) compares `hello_world` builds on CI, but its charts/collection conditions must not be silently imported as Wirescape measurements.

**Repeatable shell protocol for a runner with dependencies:** pin Tauri/Electron versions, same `<500-byte` DOM and same host stub, release optimized production builds; report `.msi`/NSIS and `.deb` bytes, installed directory bytes, **full process-tree proportional set size/PSS on Linux or private bytes/working-set on Windows**, and 10 launches each for cold (controlled cache state) and warm start using `DOMContentLoaded` and first-paint marker. Idle = median memory 30–60 s after first paint, no inspector; test with and without C++ host; report system WebView2/WebKitGTK/Chromium versions, GPU backend and OS image. Compare only same OS/hardware; cold-start cache purges must be explicitly documented. Gate signing and updater rollback independently.

## 4. Front-end choice: vanilla JS versus frameworks

**Measured/observed constraint:** the optional single-file owner sketch is described as vanilla HTML/CSS/SVG/JS with draggable modules, typed cables, knobs, pan/zoom and JSON import/export, but **was not attached**. No claim is made about its source code or pixel-level behavior. Current QML already performs geometry and presentation without engine interaction, while the native host owns accepted edits and persistence (`native/ui/studio_controller.cpp:298-316`; `docs/research/ENGINE_PROTOCOL.md@research/engine-ui-protocol:92,153-183`).

| Option | Fit for first implementation | Tradeoff and evidence |
|---|---|---|
| **Vanilla JS with ES modules (recommend now)** | Most direct continuation of sketch, no package/runtime dependency; typed geometry helpers, event delegation, DOM/SVG; portable in Windows WebView2, WebKitGTK and Pi Chromium. | Manual lifecycle, invalidation and state ownership may grow messy. Introduce a small `hostClient`, state store, geometry, input, renderer, UI panel and serialization boundary; **do not** parse/rewrite native-v2 patch bytes in browser. TypeScript is optional build-time syntax, not a new runtime framework. |
| **Svelte** | First framework escalation if panel/forms/state complexity grows: compiler converts components to JavaScript, could keep geometry renderer imperative. | Build pipeline and runtime framework semantics; benchmark on both browser engines. [Svelte docs](https://svelte.dev/docs/svelte/overview). |
| **Solid** | Fine-grained reactive panel state helps frequent knobs/meters without rerendering large trees. | Less familiar ecosystem; still keep canvas outside framework reconciliation. [Solid docs](https://docs.solidjs.com/concepts/intro-to-reactivity). |
| **React** | Large component ecosystem; appropriate for an extensive dashboard but unnecessary for current sketch. | Stateful rendering/commit phases need explicit isolation of per-frame graph geometry; not automatically faster. [React render/commit](https://react.dev/learn/render-and-commit). |

**Implementation split:** a minimal immutable *host-accepted* document snapshot with revision, local ephemeral drag coordinates separately, and a single control-owner command queue. The UI gets `state.get` snapshot and correlated `parameter.changed`, `document.changed`, `meters.changed`; it must not treat an optimistic knob movement or `accepted` reply as proof that audio rendered it. The protocol draft distinguishes revision, generation, appliedGeneration and epoch (`docs/research/ENGINE_PROTOCOL.md@research/engine-ui-protocol:185-210,221-225`). Limit meter DOM refresh initially to 30 Hz latest-only; preserve non-droppable note-off, panic and save commands.

## 5. Patch-canvas renderer: SVG versus Canvas2D versus PixiJS

| Renderer | Strength | Costs and risks | Recommendation |
|---|---|---|---|
| **SVG retained-mode DOM** | Native hit testing, accessible/focusable endpoints and text, scalable cables and CSS; direct mapping from sketch; **this local Chromium benchmark** yielded higher presentation FPS than Canvas2D. | DOM/node count, path mutation, layout/paint load, selection complexity at higher density; must virtualize/limit updates. | **Start here** for ~100 modules / 400 cables, with `pointer-events`, `touch-action:none`, wheel focal zoom and stable IDs; use delegated hit testing and path caching. |
| **Canvas2D immediate-mode** | Few DOM elements, lower per-frame JS draw time locally; can draw 100s/1000s of edges with batching. | Own hit tests, keyboard selection, text/IME/accessibility overlay and high-DPI math; full redraw/dirty region management. | Preserve a renderer interface and measure as an alternative at larger graph scale / degraded WebKitGTK. |
| **WebGL/PixiJS** | Batched GPU primitives and large scenes where WebGL actually accelerates; PixiJS WebGL renderer is its recommended stable path. | Dependency, GPU memory/shader complexity, WebGL/ANGLE/WebKit/VC4 compatibility, context loss; no native DOM accessibility; `Graphics` Bézier tessellation benchmark must be separate. | **Defer until Pi GPU evidence**; PixiJS was *not* benchmarked here. [Pixi renderer docs](https://pixijs.com/guides/components/renderers/). |

**Interaction/format requirements:** separate visual `audio/control` colors from canonical patch-v2 `signal/modulation/event` meanings; ID-stable ports, drag capture/lost-pointer handling, normalized wheel/gesture zoom around pointer, snapping, focus/keyboard note release, right-click/touch long-press and undo/redo. Do not allow a rendered cable index to become an unguarded persistent identity; host must revision-check structural edits and preserve native `modulation.amount`, feedback, stereo pairs and output routes (`docs/research/ENGINE_PROTOCOL.md@research/engine-ui-protocol:211-229`).

## 6. Pi 5 risks and explicit gates

1. **WebKitGTK GPU is not guaranteed.** Tauri/`webview` use system WebKitGTK 4.1. WebKitGTK has undergone DMA-BUF renderer changes (including removal of old renderers) and more recent accelerated Canvas2D work; these are upstream changes, **not confirmation** of working Pi 5 accelerated SVG/Canvas/WebGL. Check `webkit2gtk-driver`, `eglinfo/glxinfo`, actual compositor logs and WebKit Inspector GPU traces on the target OS; avoid unsupported environment toggles presented as production fixes. [WebKitGTK 2.44](https://webkitgtk.org/2024/03/27/webkigit-2.44.html), [2.51.90 release](https://webkitgtk.org/2026/01/23/webkitgtk2.51.90-released.html).
2. **Pi kiosk fallback is practical and testable.** Raspberry Pi OS 64-bit defaults to Wayland/**labwc** and documents Chromium `--kiosk` via labwc autostart ([Pi kiosk guide](https://www.raspberrypi.com/tutorials/how-to-use-a-raspberry-pi-in-kiosk-mode/), [config](https://www.raspberrypi.com/documentation/computers/configuration.html)). A browser must access only a restricted same-host UI endpoint; no LAN bind, no unauthenticated localhost control, no arbitrary WebSocket origins or filesystem paths. Prefer a packaged trusted app URL/localhost server only with short-lived host bootstrap auth. Kiosk session user should be unprivileged and should not auto-launch physical audio devices in tests.
3. **Touch/gestures:** test touch screen at native scale and 125/150/200% UI scale, 1- and 2-finger pan/zoom, draggable port targets, palm cancellation, wheel and pen, context-menu, keyboard chords and focus-loss release. `touch-action:none` is necessary to prevent scrolling from stealing patch gestures but must be scoped to patch surface, not every control. Determine native WebKit touch-action/PointerEvent behavior on Pi: this x86 Chromium result cannot establish it.
4. **Thermal/GPU/OS variation:** pin Pi OS build, Mesa/V3D, labwc, Chromium/WebKitGTK, display resolution and refresh; log CPU governor and throttling. Measure warm/cold start, GPU backend, 5-minute min/p50/p95/p99 frame time, render JS time, power/temperature and total PSS at 100/400, 200/800, 500/2000. Monitor a running fake-host CPU load with **no real devices**; use Pi runner only through the authorized `pi5` bridge.
5. **Acceptance proposals (not established thresholds):** require **p95 frame interval ≤25 ms** in 100/400 drag+zoom, no >100 ms sustained stalls, reliable pointer release and geometry/ID correspondence; **<250 MiB incremental UI process-tree PSS over an idle Pi desktop** as a *planning budget*, not a claim or platform minimum. Prefer per-engine CPU budget and render stability over shell MB ranking; planner may revise budgets after baseline.

## 7. Engine host/process architecture and latency

| | In-process engine in shell | **Separate C++ engine host sidecar (recommended)** |
|---|---|---|
| Ownership | Tauri Rust FFI/`webview` C++/Qt C++/Electron native addon in shell process; direct-call lowest overhead. | C++17 host owns ABI2 module loader, canonical native-v2 doc, async AudioSession and callbacks; shell never owns C++ DSP pointers; one writer control thread. |
| Failure domain | A faulty C++ plugin/UB/memory corruption terminates or corrupts the native UI and can lose unsaved state. Chromium renderer may already be a separate child, but **native engine** shares shell process. | C++ plugin/audio crash can kill *host* but leave shell to show recover/relaunch; webview renderer crash can be recreated without killing healthy host. Sidecar death still interrupts audio; cannot claim seamless recovery. |
| Transport latency | No OS IPC hop, but FFI into engine must **never** execute on realtime thread. UI → control-thread queue still required. | UI → Tauri `invoke` → Rust ACL/validation → framed local pipe/UDS → host validation/control owner → reply/event. Extra copies/scheduling; no direct native calls from web rendering or audio callback. |
| Restart/shutdown | Single-process lifetime; stale renderer references may fault without negotiated resets. | Supervise pid, reauthenticate and **increment epoch** on restart, reject stale events/revisions, requery `state.get`; on shell loss enforce explicit **stop/retain policy** to prevent orphaned audio or stuck notes; no assumption sidecars die automatically. |
| IPC options | C++ FFI into Rust via C ABI; `webview.bind` callback; `QWebChannel`; Node N-API addon. | Windows ACL-restricted named pipes, Unix domain sockets `0600`, length-prefixed UTF-8 protocol frames, or authenticated loopback WS *only* for kiosk. stdio viable for prototype but poor for multi-client/restart; never block on stdout pipes or parse JSON on audio thread. |

**Latency budget, analytically derived—not measured:** 256 frames / 48,000 Hz = **5.333 ms audio callback block** (`native/ui/studio_controller.h:88-89`; `docs/research/ENGINE_PROTOCOL.md@research/engine-ui-protocol:45-47`). A 60 Hz UI frame is **16.667 ms** and 30 Hz telemetry cadence **33.333 ms**. Even zero IPC latency cannot force a change onto already-rendered frames; command acceptance is not sample-accurate effect timing. Target **p95 ≤2 ms control-host IPC round trip for `parameter.set` using a fake backend**, and **p99 ≤5 ms** under 30 Hz telemetry with 100 knob writes/s; these are *proposed gates*, not observations. A 2 ms command RTT leaves ~14.7 ms of one 60 Hz UI frame for rendering and event processing, but OS scheduling and engine-block alignment dominate time-to-audible-change. Benchmark and timestamp at **UI request, Rust receive/send, host receive, validation, queue acceptance, reply, appliedGeneration**; never infer audio latency from RPC reply. Slow patch.load/render/demo/save must be asynchronous job operations. If no in-flight command capacity remains, preserve emergency note-off/panic path (current implementation has bounded engine ingress at `native/control/app_engine.cpp:72-89`).

**Concrete host shape:** one C++ executable with separate control and producer threads, one RAII engine owner and a parked callback before graph swap, one canonical byte-preserving native-v2 document and revision counter, bounded queues and backpressure; privileged file grants and atomic save, host-owned module catalog roots; Tauri adapter validates capabilities and marshals protocol v1; Linux kiosk adapter requires a unique one-use 256-bit session token, strict Origin/Host and loopback binding. Keep the single protocol schema and conformance tests in [draft #211](https://github.com/kalharbi11/wirescape/pull/211); do **not** reimplement the native-v2 editor in JS. The existing Qt-free audio adapter is blocking and needs a fake-testable asynchronous AudioSession (`native/io/audio/portaudio_service.cpp:70-91`). No real devices or LAN sockets in tests.

## 8. Reproducible SVG / Canvas2D benchmark and local results

**Harness construction:** deterministic 100 modules in a 10×10 grid, 400 seeded directed cubic Bézier cables (3 types); every animation frame animates module 0 and camera zoom, **recalculates all 400 paths**, and redraws/moves 100 modules. Interactive pointer drag on nodes/empty space, pointer-capture release, and wheel zoom; 1280×800 patch stage, devicePixelRatio=1. SVG uses individual `<path>` + per-node groups (DOM text/circles); Canvas2D batches edge strokes by type and redraws nodes. These are **not pixel-identical** (SVG rounded rectangle and separate DOM paths versus Canvas square fill and grouped strokes), so renderer quality and precise visual workload remain confounders. No WASM, framework, PixiJS, audio, real devices or network. This benchmark tests **client render throughput only**, not IPC/engine/host latency.

**Execution:** headless Chromium **144.0.7559.96**, Linux **x86_64** isolated container, viewport 1280×836, no headless shell download; Playwright Python using system `/usr/bin/chromium`, `page.set_content` of the identical HTML (host policy blocked local `file://` and loopback navigation), warmed up **500 ms** each, then measured for **4,250 ms** each; 6 interleaved trials (`svg,canvas,canvas,svg,svg,canvas`), ignore first **350 ms** for frame-interval stats. `requestAnimationFrame` timestamps determine FPS/p95 frame interval, JS `performance.now()` spans `render()`, Chrome DevTools Protocol reads JS heap and DOM counters after run. **GPU compositor implementation/refresh scheduler not verified, no Pi/Windows/WebKit run.** JS heap is **not process RSS**.

| Metric (median of 3 runs per renderer) | SVG | Canvas2D |
|---|---:|---:|
| FPS (`rAF` frames / measured elapsed time) | **44.43** | **32.03** |
| Median inter-frame interval | **16.70 ms** | **33.30 ms** |
| p95 inter-frame interval | **33.40 ms** | **33.40 ms** |
| Mean JS `render()` duration | **1.01 ms** | **0.42 ms** |
| p95 JS `render()` duration | **1.40 ms** | **0.60 ms** |
| JS heap used after bench | **1.22 MiB** | **1.45 MiB** |
| DOM node count (CDP) | **1,234** | **39** |

**Individual FPS trials:** SVG **44.17 / 44.43 / 50.34**; Canvas2D **32.03 / 31.41 / 32.80**. A Canvas draw function taking less CPU time but presenting fewer frames in headless Chromium indicates compositor/scheduler/graphics backend effects; **do not** extrapolate 44 FPS to Pi hardware or declare SVG inherently faster. Neither median FPS reaches a stable 60 FPS under this synthetic full-scene invalidation. WebKitGTK browser absent; Playwright WebKit executable absent; `pkg-config webkit2gtk-4.1` absent. **WebKit result: NOT MEASURED.** WebGL/PixiJS result: **NOT MEASURED** (would need a pinned vendor bundle and graphics availability). No Tauri/Electron binary measured.

**Benchmark HTML in this PR:** extract the exact fenced HTML below using this offline command (creates an *untracked* local file; a later planner-approved PR can add it under `docs/research/bench/`):

```sh
python3 - <<'PY'
from pathlib import Path
s = Path('docs/research/WEB_STACK.md').read_text(encoding='utf-8')
h = s.split('<!-- BEGIN BENCH HTML -->\n```html\n', 1)[1].split('\n```\n<!-- END BENCH HTML -->', 1)[0]
p = Path('docs/research/bench/web_renderer_100_400.html')
p.parent.mkdir(parents=True, exist_ok=True)
p.write_text(h + '\n', encoding='utf-8')
print(p, p.stat().st_size)
PY
```

Open extracted HTML with `?mode=svg` and `?mode=canvas`; call `window.runBench(4250)` in developer console or via Playwright. This exact code is also interactive manually. It includes no remote resources, device access or LAN sockets. On Pi, open from a sandboxed `file://` or trusted local app-asset scheme; avoid a host service on `0.0.0.0`.

<!-- BEGIN BENCH HTML -->
```html
<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only -->
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>WIRESCAPE renderer stress harness: 100 / 400</title>
<style>
html,body{margin:0;background:#141824;color:#eff2f8;font:13px system-ui}
#bar{height:36px;display:flex;align-items:center;gap:14px;padding:0 12px;background:#232c3c}
#stage{width:100vw;height:calc(100vh - 36px);display:block;touch-action:none;cursor:grab}
#stage.dragging{cursor:grabbing} svg,canvas{display:block}
</style>
<div id="bar"><strong>WIRESCAPE renderer stress</strong><span id="stat">100 modules · 400 cables</span>
<span>Drag nodes · wheel to zoom · drag empty space to pan</span></div>
<div id="stage"></div>
<script>
(()=>{
"use strict";
const S=document.getElementById("stage"), stat=document.getElementById("stat");
const mode=new URLSearchParams(location.search).get("mode")==="canvas"?"canvas":"svg";
const W=1280,H=800,N=100,E=400;
const rand=(()=>{let n=0x10203040;return()=>((n=(Math.imul(n,1664525)+1013904223)>>>0)/4294967296)})();
const nodes=Array.from({length:N},(_,i)=>({id:i,x:35+(i%10)*195,y:32+Math.floor(i/10)*126}));
const edges=Array.from({length:E},(_,i)=>{let a=(rand()*N)|0,b=(rand()*N)|0;if(a===b)b=(b+1)%N;return {a,b,type:i%3};});
const cablesOf0=edges.reduce((a,e,i)=>(e.a===0||e.b===0?a.push(i):null,a),[]);
const camera={x:0,y:0,z:.62};let pointer=null, canvas=null,ctx=null,svg=null,world=null,paths=[],rects=[];
const NS="http://www.w3.org/2000/svg";
function bez(e){let a=nodes[e.a],b=nodes[e.b],x=a.x+150,y=a.y+29,X=b.x,Y=b.y+29,dx=Math.max(50,Math.abs(X-x)*.5);return `M${x},${y} C${x+dx},${y} ${X-dx},${Y} ${X},${Y}`;}
function drawCable(ctx,e){let a=nodes[e.a],b=nodes[e.b],x=a.x+150,y=a.y+29,X=b.x,Y=b.y+29,dx=Math.max(50,Math.abs(X-x)*.5);ctx.moveTo(x,y);ctx.bezierCurveTo(x+dx,y,X-dx,Y,X,Y);}
function elt(tag,attrs,par){let a=document.createElementNS(NS,tag);for(const k in attrs)a.setAttribute(k,attrs[k]);par.appendChild(a);return a;}
function makeSVG(){
 svg=elt("svg",{width:"100%",height:"100%",viewBox:`0 0 ${W} ${H}`},S);
 world=elt("g",{},svg);let cables=elt("g",{"fill":"none","stroke-width":2,"opacity":".74"},world);
 paths=edges.map(e=>elt("path",{d:bez(e),stroke:["#86a6ff","#f1b96c","#64d2bd"][e.type]},cables));
 const group=elt("g",{},world);
 rects=nodes.map(n=>{let g=elt("g",{transform:`translate(${n.x} ${n.y})`},group);
 elt("rect",{width:150,height:80,rx:9,fill:"#2c3d52",stroke:"#8198b3"},g);
 elt("rect",{x:0,y:0,width:150,height:26,rx:9,fill:"#44678b"},g);
 elt("text",{x:8,y:18,fill:"#fff","font-size":13},g).textContent="MODULE "+n.id;
 for(let p=0;p<3;p++)elt("circle",{cx:18+p*45,cy:53,r:9,fill:"#b9d5e9"},g);
 return g;});
}
function makeCanvas(){canvas=document.createElement("canvas");ctx=canvas.getContext("2d");S.appendChild(canvas);}
function resize(){let r=S.getBoundingClientRect();if(canvas){const d=Math.min(devicePixelRatio||1,2);canvas.width=Math.round(r.width*d);canvas.height=Math.round(r.height*d);canvas.style.width=r.width+"px";canvas.style.height=r.height+"px";} render();}
function render(){
 if(svg){world.setAttribute("transform",`translate(${camera.x} ${camera.y}) scale(${camera.z})`);for(let i=0;i<E;i++)paths[i].setAttribute("d",bez(edges[i]));for(let i=0;i<N;i++)rects[i].setAttribute("transform",`translate(${nodes[i].x} ${nodes[i].y})`);}
 else{let d=canvas.width/S.clientWidth;ctx.setTransform(d,0,0,d,0,0);ctx.clearRect(0,0,S.clientWidth,S.clientHeight);
 ctx.translate(camera.x,camera.y);ctx.scale(camera.z,camera.z);
 for(let k=0;k<3;k++){ctx.beginPath();for(const e of edges)if(e.type===k)drawCable(ctx,e);ctx.strokeStyle=["#86a6ff","#f1b96c","#64d2bd"][k];ctx.globalAlpha=.74;ctx.lineWidth=2;ctx.stroke();}
 ctx.globalAlpha=1;ctx.font="13px system-ui";
 for(const n of nodes){ctx.fillStyle="#2c3d52";ctx.fillRect(n.x,n.y,150,80);ctx.fillStyle="#44678b";ctx.fillRect(n.x,n.y,150,26);ctx.fillStyle="#fff";ctx.fillText("MODULE "+n.id,n.x+8,n.y+18);for(let p=0;p<3;p++){ctx.beginPath();ctx.arc(n.x+18+p*45,n.y+53,9,0,Math.PI*2);ctx.fillStyle="#b9d5e9";ctx.fill();}}
 }
}
function hit(x,y){let X=(x-camera.x)/camera.z,Y=(y-camera.y)/camera.z;for(let i=N-1;i>=0;i--)if(X>=nodes[i].x&&X<nodes[i].x+150&&Y>=nodes[i].y&&Y<nodes[i].y+80)return i;return -1;}
S.addEventListener("pointerdown",e=>{S.setPointerCapture(e.pointerId);pointer={px:e.clientX,py:e.clientY,node:hit(e.offsetX,e.offsetY)};S.classList.add("dragging");});
S.addEventListener("pointermove",e=>{if(!pointer)return;let dx=e.clientX-pointer.px,dy=e.clientY-pointer.py;pointer.px=e.clientX;pointer.py=e.clientY;if(pointer.node>=0){nodes[pointer.node].x+=dx/camera.z;nodes[pointer.node].y+=dy/camera.z;}else{camera.x+=dx;camera.y+=dy;}render();});
S.addEventListener("pointerup",()=>{pointer=null;S.classList.remove("dragging");});
S.addEventListener("wheel",e=>{e.preventDefault();let z=Math.max(.2,Math.min(2,camera.z*Math.exp(-e.deltaY*.001)));let x=e.offsetX,y=e.offsetY;camera.x=x-(x-camera.x)*z/camera.z;camera.y=y-(y-camera.y)*z/camera.z;camera.z=z;render();},{passive:false});
function percentile(a,p){let b=a.slice().sort((x,y)=>x-y);return b[Math.min(b.length-1,Math.floor(p*(b.length-1)))];}
let running=false;
window.runBench=ms=>new Promise(resolve=>{
 if(running)throw Error("already running");running=true;
 const samples=[],cpu=[];let t0=0,prev=0,frames=0;
 const step=now=>{if(!t0){t0=prev=now;requestAnimationFrame(step);return}
 const elapsed=now-t0;
 // Continuous module drag and zoom; force every cable geometry update in both modes.
 nodes[0].x=35+Math.sin(elapsed*.002)*110;
 nodes[0].y=32+Math.cos(elapsed*.0017)*60;
 camera.z=.62+Math.sin(elapsed*.0009)*.095;
 let start=performance.now();render();cpu.push(performance.now()-start);
 if(elapsed>=350&&prev>0)samples.push(now-prev);
 frames++;prev=now;
 if(elapsed<ms){requestAnimationFrame(step)}else{
 running=false;let duration=elapsed-350,valid=samples.length;
 const result={mode,nodes:N,cables:E,drawsPerFrame:E,frames:valid,fps:Number((valid*1000/duration).toFixed(2)),
  medianFrameMs:Number(percentile(samples,.5).toFixed(2)),p95FrameMs:Number(percentile(samples,.95).toFixed(2)),
  meanRenderMs:Number((cpu.reduce((a,b)=>a+b,0)/cpu.length).toFixed(2)),
  p95RenderMs:Number(percentile(cpu,.95).toFixed(2)),
  userAgent:navigator.userAgent,devicePixelRatio,viewport:[S.clientWidth,S.clientHeight]};
 stat.textContent=JSON.stringify(result);window.lastBench=result;resolve(result);
 }};
 requestAnimationFrame(step);
});
if(mode==="svg")makeSVG();else makeCanvas();window.addEventListener("resize",resize);resize();window.BENCH={mode,nodes,edges,draw:render};
})();
</script>
</html>
```
<!-- END BENCH HTML -->

## 9. Proposed acceptance matrix and rollout (unexecuted)

1. **Windows shell CI:** C++ host stub with fake audio/MIDI + Tauri 2 package NSIS and MSI, WebView2 present/absent, clean Win10/11 VM, installer sidecar inclusion and Authenticode signatures; explicitly run permission checks on native commands, no arbitrary URLs or paths; update signature verification + rollback/recovery. Electron hello-world reference build only for like-for-like A/B size/RSS/start.
2. **Linux ARM64 Pi offline CI:** pinned Raspberry Pi OS 64-bit / labwc, WebKitGTK 4.1 native Tauri `.deb` *and* system Chromium kiosk A/B using same assets and same host fake backend; record WebKit process tree, CPU/GPU renderer, rAF p95/p99, touch input events, screen-resume and thermal changes. AppImage optional, native ARM build required.
3. **Renderer regression suite:** 100/400 baseline, 200/800 and 500/2000 growth, dragging, wheel/trackpad pinch, two-finger touch, mixed edge types, keyboard focus-loss panic and disconnect; per-browser screenshots, hit-testing and stable IDs. Use exact protocol fixture schema from #211 (rather than changing ABI/module IDs), call accepted host operations, test conflict/reconnect/dirty document and lossless file round trips with unknown fields.
4. **Realtime/IPC safety:** fault-inject native host crash, shell/renderer crash and restart, bounded telemetry and priority note-off queue, host mutex/block/alloc probes, TSAN/ASAN fake callback tests, 30/60 Hz stream stress, no devices/LAN; only loopback where the transport test explicitly requires it. App UI cannot block PortAudio render; stop/shutdown race must not free callback-owned memory.
5. **Release decision:** do not switch production shell until Windows/Pi budget evidence, code signing, updater recovery, licence/SBOM review and preservation-capable editor migration are documented. All proposed limits in this document require planner approval. No production-ready or gate claim is made.

## Decisions needed from the planner

1. **Windows shell:** **A)** Tauri 2 + supervised C++ host, **B)** `webview` C++ host, **C)** Electron + C++ host. **Recommend A**, with a small Electron control build for objective Windows size/RSS/start comparison before final release selection.
2. **Pi UI deployment:** **A)** system Chromium kiosk over authenticated loopback host, **B)** Tauri 2 `.deb` + WebKitGTK 4.1, **C)** Electron ARM64. **Recommend A for initial Pi measurement**, keep B conditional on GPU/touch parity and C as last-resort Chromium-bundled escape hatch.
3. **Renderer:** **A)** SVG v1 with Canvas2D fallback interface, **B)** Canvas2D default, **C)** PixiJS/WebGL now. **Recommend A**, subject to native Pi WebKit and Chromium p95 frame-time gates; local x86 data alone cannot certify it.
4. **Front-end programming model:** **A)** vanilla ES modules (optional TypeScript compile) now, **B)** Svelte components after complexity threshold, **C)** React/Solid immediately. **Recommend A**, with B as the first framework experiment for complicated panels; do not move graph geometry into reactive DOM reconciliation.
5. **Engine placement/transport:** **A)** standalone native host with OS-restricted pipes/UDS and optional token-authenticated loopback WS, **B)** in-process Rust/C++ FFI, **C)** WebSocket-only service. **Recommend A** for DSP/plugin crash isolation, one host-owned document and strict RT separation.
6. **Measurements before implementation commitment:** **A)** require like-for-like Windows installers/idle RSS/cold-start and Pi Chromium/WebKit/touch/GPU benchmarks, **B)** rely on third-party shell figures, **C)** decide from this x86 Chromium test alone. **Recommend A**; proposed budgets are research gates, not achieved results.
7. **Benchmark file placement under one-file authorization:** **A)** keep extractable HTML embedded here and authorize a later dedicated `docs/research/bench/` file, **B)** explicitly expand pause exception to permit a second new file in this PR, **C)** omit the harness. **Recommend A now** to obey the one-new-file pause limit without losing reproducibility; B only with explicit owner/planner expansion.