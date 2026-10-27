> Historical approved foundation rationale. For current implementation, validation, changed priorities and review work, start at ../START_HERE_CURRENT_BUILD.md and docs/CURRENT_*.md. Statements below about planning-only/no implementation are historical.

# Wirescape: revised foundation plan

Revised October 26, 2026 after the standalone-app and efficiency brief. **Planning deliverable only. Implementation has not started.**

## Recommendation

Build Wirescape as a **standalone Windows application and a standalone Pi application**, with a portable processing core that also runs without a graphical interface. Preserve the Studio visual identity, CHAOS's musical purpose, existing patch data, and sequencer functionality. Their present code and execution strategy are open to improvement.

The recommended target is a **native C++ processing engine, Qt 6 / Qt Quick interface, and optional Python authoring/control/data layer**. Keep the engine independent of Qt and Python. Use a narrow language-neutral interface so compiled Python kernels, PyPy experiments, native modules, and future frontends can share it. This is a proposed architecture, not a measured claim that a language change by itself produces a speedup.

The major changes to the previous plan are:

1. Make desktop/Pi deployment and native GUI feasibility an early gate. Rebuild the interface around native rendering while retaining its design language.
2. Treat old benchmarks as evidence about the old implementation. Set fresh workloads and compare new implementations at matched quality and latency.
3. Make processing efficiency a core design constraint: reduce work, memory traffic, allocations, dispatch, and language crossings before simply adding threads.
4. Evaluate PyPy, compiled Python, and native kernels fairly. Choose winners per component rather than committing the entire application to one interpreter.
5. Optimize CHAOS's parameter resolution, buffers, DSP organization, and engine arrangement; the two-Surge implementation is a testable design choice.
6. Protect documents and establish stable module boundaries, then prove real modules before freezing SDK v2.

The ambition is to exceed strong existing tools on well-defined workloads. No existing DAW or old Wirescape result is the performance ceiling. Progress is demonstrated by new measurements, supported patch complexity, and musical quality. There is no universal runtime switch that guarantees maximum efficiency for every workload.

The next foundation release is provisionally **0.6**. The first work is a focused feasibility/measurement stage, followed by a staged implementation. Completing the entire long-term module wish list is not a prerequisite for a useful foundation.

## Scope and evidence

The review covers the four supplied handoff documents, project source and documentation, module builder kit and template, tests/examples, browser code/design reference, benchmark reports and raw tables, and benchmark tooling. Virtual-environment dependencies, caches, and the native binary are environment artifacts, rather than source to redesign.

The handoffs provide history, intended behavior, and proposals. They are not implementation authorization. The current request governs: research and present the final plan first. The latest standalone/efficiency requirements supersede earlier browser-host and preserve-current-implementation assumptions.

No application code, configuration, personal data, or native binary was changed. No dependency installation, benchmark rerun, audio playback, or hardware test was performed for this review. The 71-test result below belongs to the supplied baseline, not a new verification run.

### Findings that change the plan

Repository paths in this document are relative to `<repo>/`.

| Observed evidence | Consequence for the plan |
|---|---|
| The normal launcher runs `py -3.12 -m wirescape`. There are also divergent Python host files at the project root. Root `__init__.py` says 0.3.0, nested `wirescape/__init__.py` says 0.5.0, and the active server reports 0.5.2. | First capture provenance and designate the nested package as the canonical launch/import path. Preserve and compare the stale copies before removing or quarantining them. |
| The browser exports patch **version 3** (`web/index.html:500`), while older examples/docs describe version 1. Host validation does not dispatch by patch format/version (`wirescape/schema.py:54`). | Compatibility must cover the actual historical formats and migrations. Do not design a fictional single-version baseline. |
| Browser import rebuilds only recognized modules and compatible cables, then saves (`web/index.html:502`, especially `:524`). The host rejects unknown module types (`wirescape/schema.py:79`). Startup deferral helps only some paths. | Add a lossless document reader and missing-module placeholders first. Unsupported modules must not destroy saved state or unresolved cables. |
| On connection/reconnection the browser sends its local patch (`web/client.js:123`, `:141`). The host's `last_patch` is not a complete canonical document updated by every incremental command. | Give the new application document controller authority over accepted state and recovery. This identifies an ownership problem to avoid in the native app; repairing the retiring browser transport is not a prerequisite. |
| Validation calls plugin activation (`wirescape/schema.py:63`). Registration mutates globals before success is established (`wirescape/extensions.py:62`, `wirescape/plugins.py:27`). Invalid manifests can disappear from the catalog (`extensions.py:24`, `:81`). | Make document validation pure, registry activation transactional, and failed discoveries visible. |
| The graph drains commands without a work limit and rebuilds routing/buffers every block (`wirescape/graph.py:224`, `:320`). | Prepared routing and bounded command processing remain high-value changes. |
| CHAOS has another command queue/drain, lazy render-buffer creation, and a shared analysis lock (`modules/chaos/engine.py:269`, `:403`, `:466`). Its UI reader rolls the full analysis ring under that lock (`:480`). | A host-only cleanup is incomplete. Repair the engine's host-facing execution and analysis handoff while preserving sound behavior. |

### What the benchmark establishes

The two benchmark directories contain the same ten files, checked by SHA-256. They are one evidence set, not independent replications. The handoff's full source/benchmark ZIPs and checksum manifest were not available here as a complete provenance package; the current folder must be captured as its own baseline.

The supplied report records Windows 11, i7-14700HX, Python 3.12.10, real Surge `1.4.main.a30341b`, 48 kHz, 512 frames, and **71 passing tests**. All performance renders were offline. No live device stability or Pi throughput was established. These results describe that build; a redesigned engine, GUI, and CHAOS can have very different costs and capacity.

At these settings, one block represents **10.667 ms**:

| Workload | Reported median block | Reported p99 block |
|---|---:|---:|
| One CHAOS, four notes, clean preset | 4.30 ms | 11.54 ms |
| One CHAOS, four notes, complex preset | 5.96 ms | 13.36 ms |
| Two CHAOS, four notes each, complex preset | 12.23 ms | 26.81 ms |
| Three CHAOS, four notes each, clean preset | 13.13 ms | 34.12 ms |

These are the report's summaries of three runs, not pooled block distributions. Every active-note single-CHAOS case has a reported p99 above the nominal deadline. This is a timing concern, not evidence of an observed audible dropout. CSV `ok` and table `pass` mean successful rendering, not realtime readiness.

One profiled configuration assigns 62.5% of measured time to Python-facing Surge render calls, 27.8% to other CHAOS processing, and 9.6% to the outer graph. These shares diagnose one old workload. They neither predict the new engine nor set its optimization ceiling: both the graph and the instrument internals can change.

The next measurement tools should correct these ambiguities:

- `creation_preset_load_ms` excludes the preset-file read and much queued application work (`tools/bench/run_baseline.py:84`).
- `parameter_drain_ms` includes a normal render, rather than isolating command cost (`:182`).
- Traced peak memory is a run peak, not per-block allocation (`:175`). Allocation tracing must be separate from normal timing comparisons.
- Output finiteness is checked after sanitization (`wirescape/graph.py:419`, `:436`). Count upstream non-finite results before sanitizing.
- Current `render_ms` excludes later meter/report work (`graph.py:437`). Measure the whole device callback separately.
- Current `xruns` conflates device status and Python faults (`wirescape/audio.py:45`). Split underflow, overflow, callback exception, and device failure counters.

## Foundation architecture

```text
Standalone Qt Quick application
  native canvas, generic/custom module editors, input, presentation
                    |
Application document and control services
  accepted patch + revisions, registry, parameters, transport, devices,
  migration, persistence, validation, graph preparation, diagnostics
                    |
          +---------+--------------------+
          |                              |
Native headless engine            Asynchronous providers/workers
  prepared audio + timed          MIDI/OSC/serial, parsing,
  events + synchronous control    Python scripting, slow data/files
          |                              |
          +---- bounded explicit bridges-+
```

This keeps one visual patch while respecting different timing requirements. Initially there are two execution boundaries, not a separate scheduler for every port kind. Audio-affecting control and events share the processing timeline; devices/data use asynchronous providers with bounded bridges.

The processing core must load/render/run native-only patches without a display, Qt event loop, browser, HTTP server, or Python interpreter. Patches using optional Python/Numba providers declare that runtime dependency explicitly; headless operation can load it when required. The application uses typed in-process commands and snapshots. Document revisions and instance generations remain useful; internal JSON/WebSocket serialization is unnecessary. Optional external control can later be another adapter.

Use a thin native Qt application facade by default. Python remains available for productive module development, graph preparation, automation, and providers where measurements support it. Any interpreted audio plugin has an explicit execution/latency tier; it must not silently pull the bounded native processing path through the interpreter. A PySide6 shell is a viable lower-effort experiment if it materially simplifies development, but the engine must remain independently runnable.

For MIDI-only/control-only patches, the host can drive the musical timeline from a monotonic clock while audio is stopped. Audio-start/stop handover must be explicit and tested. A physical audio stream must not be required merely to route a sensor or send MIDI.

The separation is consistent with mature audio systems' preparation/data-processing boundary; PipeWire, for example, prepares buffers and metadata before scheduling its processing threads. Wirescape should borrow that principle without adopting PipeWire's entire scheduler or making it a Windows dependency. [PipeWire scheduling](https://pipewire.pages.freedesktop.org/pipewire/page_scheduling.html)

### 1. Document ownership and compatibility

- An application document controller owns one accepted `PatchDocument`, independently of the UI and processor. Native views receive snapshots and accepted updates.
- Commands carry request identity, document revision, and instance/graph generation where relevant. The host acknowledges acceptance or returns a useful rejection; stale commands cannot affect replacement instances.
- Initially use one active editor with simple revision/recovery handling. No collaboration engine, CRDT, or event-sourcing platform is required.
- Keep canvas position, zoom, and selection separate from processing topology. Moving a module must not reconstruct its synth.
- Preserve unknown module payloads, unknown fields, and unresolved cables in the document. A missing plugin becomes a visible placeholder; it does not execute code or emit notes. Its audio contribution is silent, and removal/disable triggers source-owned note cleanup.
- Dispatch readers by `(format, version)`, then migrate through explicit, idempotent steps. The original design's `wirescape.visual.study` version 4 (`design_reference/WIRESCAPE_Studio_Original.html:298`) is distinct from `wirescape.patch` versions 1–3; version 2 also appears in `tests/test_sequencer.py:13`. Keep the original file and write upgrades as new/atomic saves with recovery backups. A newer unsupported document must be left intact and reported clearly.
- Preserve CHAOS front values, RAW and advanced overrides, presets, keyboard targeting, sequencer `module_state`, and legacy `sequence`. Compatible optimization should preserve behavior; intentional algorithm changes require explicit state/engine versions and a documented migration or legacy interpretation. Existing DSP code is not permanently frozen.

### 2. Small versioned SDK

Keep these versions separate: patch format, module-state schema, application command contract, SDK major, module release, custom UI contract, and native processor ABI. Stable plugin/port/parameter IDs are independent of labels and folder names. Keep legacy short IDs as aliases.

Use one validated descriptor source for ports, parameters, capabilities, and UI metadata. Avoid manually duplicating definitions across manifests, processor code, and QML. The manifest describes discovery and execution requirements; the SDK validates the descriptor before activation.

The initial lifecycle covers discovery, validation, creation, preparation, activation, processing, deactivation, and disposal. Every method declares its thread owner. Include reset, bypass behavior, sample-rate/buffer re-preparation, offline rendering, and latency/tail declaration. Fix audio port layout and buffer requirements while active; structural changes require re-preparation. These are useful precedents from [CLAP lifecycle](https://github.com/free-audio/clap/blob/main/include/clap/plugin.h) and [VCV module behavior](https://vcvrack.com/manual/PluginGuide).

Provide v1 state/manifest readers and a reference/development adapter. The bundled sequencer and CHAOS gain native or certified compiled implementations during migration. Existing arbitrary Python processors cannot automatically acquire a native realtime guarantee; support their documented development/offline or explicitly latency-bearing worker tier. Registry changes commit only after successful load. Failure cleanup is best-effort for trusted Python side effects; registry rollback is not a claim that arbitrary imported code can be undone.

Use a small C-compatible interface for prepared buffers, event batches, processor handles, and lifecycle operations; keep Python/Qt/C++ object layouts behind it. This permits native and compiled-Python implementations without exposing compiler-specific objects as the module ABI. Keep it experimental until Windows and ARM builds plus reference modules exercise it. Avoid a large universal virtual machine or compiler framework in the first release.

### 3. Signals and parameters

- Audio uses float32 channel buffers with declared ownership, frame count, sample rate, and channel layout. Preserve current L/R port identities through adapters; multichannel capability does not require immediately replacing every stereo cable.
- Control describes range, units, and rate. Distinguish scalar/block control from audio-rate buffers. A block-rate cutoff input must not be advertised as audio-rate FM.
- Notes/triggers carry ordered timestamps and identity. General data carries a schema/type identity on the asynchronous side. Do not put arbitrary JSON in the realtime path.
- Make nontrivial conversions explicit: envelope following, event-to-gate, unit/range mapping, and channel conversion. Provide sensible same-type connections without forcing adapter modules for trivial cases.
- Parameter descriptors define stable IDs, bounds/defaults, units/scaling, enum values, smoothing, and modulation policy. Preserve high-resolution internal values; MIDI conversion happens at the edge.
- Separate saved base values, transient modulation, and mapped/automated changes. Declare precedence. Retain CHAOS's existing RAW/macro/front-control rules in its adapter and return accepted state to the UI.
- Exposing a parameter socket changes its presentation, not its permanent target identity. Hiding that socket must not silently delete an existing connection.

CLAP's distinct parameter-value/modulation messages and permanent parameter IDs are useful semantics to borrow. Implementing those ideas does not require hosting CLAP plugins. [CLAP parameters](https://github.com/free-audio/clap/blob/main/include/clap/ext/params.h)

### 4. Time and overload behavior

Use an always-advancing audio-frame position, musical transport position, and monotonic acquisition timestamp with explicit conversion. Distinguish scheduling time from expected DAC presentation time. Sample-rate changes/restarts need a timebase generation, so old queued events cannot be replayed against the wrong stream.

Host event batches have sample offsets, deterministic ties, and source/channel/note identity. Preserve producer order; define reset/stop ordering and off-before-on behavior for independently merged retriggers. Maintain bounded active-note ownership, including overlapping sources playing the same pitch. This borrows useful event structure from [CLAP events](https://github.com/free-audio/clap/blob/main/include/clap/events.h).

| Traffic | Initial policy |
|---|---|
| Knobs, continuous sensors, UI meters | Coalesce to the latest value by stable target; cap rate and queue size. |
| Notes, triggers, musical clock | Ordered bounded queues; explicit late-event policy, bounded per-block work, overflow counters. |
| Stop, source release, panic | Reserved bounded handling plus active-note reconciliation on loss/overflow. Never rely on note-off priority alone to survive unlimited traffic. |
| Patch/lifecycle/state changes | Validate and prepare outside processing; acknowledge, reject, or stage. Do not feed arbitrary heavy `configure()` calls to the callback. |
| Diagnostics and analysis | Bounded snapshots/ring buffers; reduce publication rate; count dropped telemetry. |

Internal scheduling precision and a destination's precision are different capabilities. Verify the selected native Surge revision's block granularity and note/control behavior. The old Python engine has block-count-dependent jitter and envelope smoothing (`engine.py:362`, `modlab.py:120`); smaller subdivisions change behavior unless coefficients become time-based or a legacy control quantum is retained. Test 32/64/128/256/512 frames and device-size adaptation. A timestamp alone does not establish sample-accurate synthesis.

Mido's message `time` field does not itself schedule device delivery, and its discovery/open/close operations need serialized ownership. Wirescape therefore owns timestamping, queueing, and device lifecycle. [Mido message timing](https://mido.readthedocs.io/en/stable/messages/#the-time-attribute), [Mido ports](https://mido.github.io/mido/ports/index.html)

OSC timetags are another timebase, not automatic network clock synchronization. Conversion, lookahead, late packets, and packet limits belong in the provider. [OSC specification](https://opensoundcontrol.stanford.edu/spec-1_0.html)

### 5. Prepared execution and resource ownership

Compile validated topology, routing indices, mixing operations, processor order, channel layouts, event capacities, and reusable scratch/output buffers outside the callback. Begin single-threaded. Parameter-only and view-only updates do not recompile topology.

An immutable plan still references mutable processors. Give each active processor one execution owner. Prepare heavy replacement state off-thread, then transfer it at a bounded boundary. Acknowledge graph generations and reclaim old plans/engines on the control side after the executor releases them. Dropping the last Python/native reference on the callback can run a destructor there; a pointer assignment alone is not safe retirement.

Keep topology edits requiring a stopped stream until bounded live activation is proven. Later swaps need defined note cleanup, tail and discontinuity behavior. Avoid a default whole-graph crossfade that renders reused mutable processors twice or doubles peak CPU.

Include CHAOS's internal queue, analysis history, label refresh, lazy buffers, and recording path in the audit. Separate equivalent optimizations from deliberate sound-design changes. Publish compact snapshots from the processor owner; format displays away from processing.

Preallocate any device-to-engine block adapter, including duplex alignment. Initially keep the existing 48 kHz/512 configuration as the reference; negotiate other supported settings after the adapter and Surge precision tests. Declare added buffering latency. Do not enable variable callback sizes without accounting for the engine's render quantum.

Initially reject zero-delay cycles. If feedback is included in 0.6, support only explicit edges delayed by at least one processing block and exclude those dependencies from the current-block DAG. At the reference setting this is 10.667 ms. One-sample/sub-block feedback requires later scheduling work; placing a Delay module inside today's DAG does not solve it by itself.

Declare latency and tails now; defer general automatic latency compensation. Schedule active MIDI/network side-effect sinks even when they do not reach an audio output. Define per-plugin failure output and note cleanup; route normal diagnostics out of the callback. Trusted native crashes or a non-returning plugin cannot be contained by catching a Python exception.

The default device callback and critical processor graph execute native code with prepared storage and bounded work. Retain the old Python runner as a diagnostic/reference implementation. PortAudio's callback constraints still apply to C++: changing language does not make allocation, locking, file access, or unpredictable code safe. [PortAudio callback rules](https://files.portaudio.com/docs/v19-doxydocs/writing_a_callback.html)

### 6. Devices, module installation, and UI

Separate a small host device registry from independently packaged protocol providers. Providers own discovery, ports, parsing, reconnect, and their worker lifecycle; the registry exposes named endpoints and status. Device-input/output modules are patchable views of those services.

Device profiles store identity, labels, calibration, and reconnect preference. Patch mappings store destinations, scaling, smoothing, and pickup. Use durable identities where a backend supplies them and explicit user matching otherwise; neither MIDI port labels nor raw UDP guarantee unique hardware identity.

Implement providers sequentially: MIDI first, then OSC/UDP or serial for the first actual Pico workflow. Use mature libraries for protocol parsing. A simulated provider and OSC loopback are enough to prove the asynchronous SDK boundary before physical integration. Declare CPU/rate limits; another Python thread is not isolation from GIL pressure. Add a worker process only when a measured workload requires it.

Keep processor descriptors and state independent of rendering. Generic parameter/port/editor descriptions work in the desktop app and headless tools. Custom native QML packages use a separately versioned UI facade with mount/unmount and subscription cleanup. A missing custom editor falls back to generic controls. The current JS/CSS custom editors need a port; retaining their data and workflow does not make those assets native automatically.

Manifest discovery must not execute code. Activation remains an explicit trust decision for executable native/Python modules and custom UI. Report dependency/API errors before activation; require safe restart for imported-code upgrades. Do not add automatic per-plugin pip installs, a dependency solver, a marketplace, or native hot-reload to the first release.

Keep user state and device profiles outside replaceable application files in the eventual package. Preserve the existing `user_data` location through an explicit migration instead of moving it silently. Lock and record the tested Windows environment, native binary hash, and source revision for reproducibility.

### 7. Standalone GUI and deployment

**Preferred UI: Qt 6 / Qt Quick/QML.** It provides a native scene graph for the spatial canvas, controls, and touch-capable Pi interface. Preserve the cream/gray/terracotta Studio design, physical-module feel, cables, and deep editing workflow. This is a native interface port, with no required browser or WebEngine component. [Qt scene graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)

Use native-backed QML shapes initially and profile a dense patch. Qt recommends combining paths and documents retessellation costs; a native custom cable layer is available if needed. Share the pan/zoom transform, invalidate only affected cable geometry, cull offscreen modules, lazily construct RAW editors, and publish decimated meters. Avoid repainting the complete canvas through Python. [Qt shapes](https://doc.qt.io/qt-6/qml-qtquick-shapes-shape.html)

GUI and audio have independent budgets. Aim for a responsive 60 Hz desktop view; allow a measured lower-refresh Pi presentation mode where useful. Measure frame-time tails, input response, idle CPU/GPU, and audio under dense-canvas interaction. A native GUI does not automatically reduce synthesis cost. [Qt performance guidance](https://doc.qt.io/qt-6/qtquick-performance.html)

Qt Widgets/QGraphicsView is a mature fallback if editor tooling wins in the prototype. Slint is worth watching, but its Python API is explicitly beta; it is a weaker default for a stable Python-facing SDK today. [Slint Python status](https://docs.slint.dev/latest/docs/python/)

Ship an installable application with the required runtime/assets, rather than requiring users to install Python and open localhost. Build Windows x64 and Linux ARM64 packages separately. Pin the actual Pi OS, graphics backend, Qt/toolchain, libc, and native DSP build early. Qt documents Pi 5 as an ARM reference configuration, but that does not validate every Pi image or wheel. Test a packaged window/canvas plus audio on the physical device before committing to the entire UI port. [Qt platform requirements](https://doc.qt.io/qtforpython-6.10/overviews/qtdoc-supported-platforms.html)

If the PySide6 shell wins the development tradeoff, its deployment tool can bundle a standalone application. Treat compilation/bundling as deployment work; any processing speed benefit needs its own measurement. [PySide deployment](https://doc.qt.io/qtforpython-6/deployment/deployment-pyside6-deploy.html)

## Efficiency research and experiments

The recommendations below are engineering hypotheses until tested in Wirescape. Optimize CPU work at an explicit quality and latency; a lower CPU percentage achieved by extra buffering, missing tails, or less processing is not an equivalent result.

### Techniques borrowed from other fields

| Source of ideas | Application to Wirescape |
|---|---|
| Compilers/dataflow systems | Compile the patch into a compact schedule. Propagate constants, track dirty dependencies, specialize known utility operations, and reuse scratch storage according to buffer lifetimes. Fuse certified primitives where semantics permit; opaque third-party modules remain valid boundaries. |
| High-performance computing/game engines | Keep hot numeric state contiguous; compare structure-of-arrays for independent voices/channels, SIMD, pooled buffers, and batched boundary crossings. Minimize memory passes and copies as well as arithmetic. |
| Databases/concurrent systems | Stable IDs, explicit migrations, transactional state replacement, generation-based publication, and deferred reclamation. Borrow mechanisms without importing an entire database/event-sourcing framework. |
| Retained graphics/reactive interfaces | Recompute only changed UI/data results. Publish snapshots and dirty notifications; keep presentation work out of processing. Audio samples still require continuous computation when their state evolves. |
| Embedded/realtime systems | Explicit budgets, bounded storage/work, overload policy, single resource ownership, cold-start preparation, and power/thermal measurements. |

Halide demonstrates separation of algorithm and scheduling; XLA demonstrates fusion and planned memory use. Borrow the principles without adding a machine-learning compiler or GPU runtime to the first core. [Halide scheduling](https://halide-lang.org/docs/tutorial/lesson_05_scheduling_1.html), [XLA fusion](https://openxla.org/xla/gpu_architecture)

SIMD is a tool, not a guarantee. Prefer compiler diagnostics and measured layouts; recursive feedback samples cannot simply be computed independently. Start with independent voices/channels and simple vector primitives. NumPy already dispatches SIMD kernels, so repeating small NumPy expressions can still lose to a fused loop through extra dispatch and memory traffic. [LLVM vectorizers](https://llvm.org/docs/Vectorizers.html), [NumPy SIMD](https://numpy.org/doc/stable/reference/simd/index.html)

### Python and native candidates

| Candidate | Fair experiment and role |
|---|---|
| **CPython reference** | Clean prepared/in-place implementation of the same kernels and graph/event workloads. Establish what algorithm/layout fixes achieve before crediting another runtime. The existing 3.12 binary remains a legacy reference, not a permanent ABI constraint. |
| **PyPy** | Benchmark warmed pure-Python numerical kernels, control/event/graph algorithms, and coarse native calls through CFFI. PyPy recommends CFFI and documents CPython-extension emulation overhead. Rebuild a suitable native bridge instead of assuming today's `.pyd` must constrain the experiment. Measure JIT warmup, GC tails, total CPU, and packaging. [PyPy FAQ](https://doc.pypy.org/faq.html) |
| **Numba** | Compare numeric kernels compiled before activation, including C-callable `cfunc` kernels invoked directly by the engine. This can retain Python authoring without entering Python per kernel call. Verify allocation/exception behavior and code lifetime. Account for CPU-specific cache/invalidation rules; do not base the distribution SDK on deprecated-in-progress `pycc`. [C callbacks](https://numba.readthedocs.io/en/stable/user/cfunc.html), [caching](https://numba.readthedocs.io/en/stable/developer/caching.html), [pycc status](https://numba.readthedocs.io/en/stable/user/pycc.html) |
| **Cython** | Compare typed numeric loops/direct native integration and exported kernels. Memoryviews can avoid Python buffer-access overhead; releasing the GIL alone does not accelerate serial arithmetic, and exception handling can reacquire it. [Memoryviews](https://docs.cython.org/en/latest/src/userguide/memoryviews.html), [GIL behavior](https://docs.cython.org/en/latest/src/userguide/nogil.html) |
| **Free-threaded CPython** | Test truly parallel control/data workloads and library compatibility. Internal locks, memory cost, and unsupported extensions remain relevant. It is not a substitute for bounded native processing. [Free-threading documentation](https://docs.python.org/3/howto/free-threading-python.html) |
| **C++ native kernels/engine** | Recommended default processing target and comparison baseline. Direct Surge ownership, predictable storage, and a common Windows/ARM toolchain fit the project. Native code still requires careful algorithms and lifetime design. |
| **Faust / ISPC** | Optional focused comparisons for DSP specialization or SIMD across independent voices/channels. Keep these authoring tools optional; add one only when it wins on a real kernel and deploys cleanly on both targets. [Faust optimization](https://faustdoc.grame.fr/manual/optimizing/), [ISPC](https://ispc.github.io/ispc.html) |

Begin with three representative kernels and two engine workloads, not complete parallel rewrites of the app. Select a runtime/compiler per component based on end-to-end results. Do not require every listed technology to remain in the product.

A C-compatible function pointer is not automatically a portable, Python-free plugin artifact. Numba kernels normally require a runtime-backed loader to create and retain their code; a Cython Python extension can also retain interpreter dependencies. Declare and package those requirements, compile/load before activation, and keep code/dependencies alive until off-thread disposal. Raw function pointers remain within one process; worker boundaries use an explicit buffer/message protocol. Publish only the compiler/deployment recipes actually proven by the SDK experiments.

Start native device I/O with a thin backend such as the existing PortAudio dependency used directly. JUCE's graph is a useful comparison/reference if it reduces engineering cost, rather than a performance ceiling or mandatory GUI/SDK. SuperCollider is an architectural reference rather than the replacement product; libossia is an optional future device-tree adapter. [JUCE graph](https://docs.juce.com/develop/classjuce_1_1AudioProcessorGraph.html), [SuperCollider architecture](https://docs.supercollider.online/Reference/Server-Architecture.html), [libossia](https://ossia.io/ossia-docs/)

Default to efficient single-threaded processing, then test coarse static parallel work only when synchronization is cheaper than the work saved. Avoid automatic thread pools in small DSP kernels. GPU/offline acceleration can be evaluated for large spectral/convolution/visual workloads; it is not the assumed solution to small live blocks.

### CHAOS improvement program

CHAOS remains a complete distinctive instrument. Its current internal organization is open to optimization and redesign.

1. **One final parameter resolver.** Existing code already caches some macro writes (`engine.py:206`); improve it rather than claiming caching is absent. Prepare native IDs/ranges/types, compile parameter dependencies, resolve front/modulation/advanced/RAW precedence once, and batch final changed values. RAW currently re-queries metadata and rewrites values per block (`:138`, `:395`). Unify cache ownership and test override clear/restore so a cached macro cannot leave a stale RAW value in the synth.
2. **Separate presentation.** Move formatted parameter labels (`:195`, `:414`) and expensive analysis reads outside processing. Publish numeric snapshots only as needed. Use bounded recording/analysis storage and explicit overflow behavior.
3. **Use circular storage and fused kernels.** Replace history concatenation in `modules/chaos/chaos_dsp.py:24` with ring buffers. Reuse looper/interpolation storage; compare fused gain/pan/mix/DC-block/saturation kernels against the existing NumPy/SciPy path. Existing filters already use compiled `lfilter`; layout and orchestration still matter.
4. **Skip only work proven unnecessary.** A dry looper with zero playback mix/feedback can skip unused playback interpolation while still recording and advancing state. Muting a layer does not prove its oscillator/envelope/tail can stop. Test transitions and hidden state; do not achieve savings through silent loss of behavior.
5. **Direct native Surge integration.** Pin the source revision and pass prepared events/buffers through a native wrapper. Upstream already batches native processing; remove avoidable Python mediation rather than introducing per-sample calls. The installed binary's exact GIL behavior is unverified. [Surge binding source](https://github.com/surge-synthesizer/surge/blob/main/src/surge-python/surgepy.cpp)
6. **Test a different engine arrangement.** Compare two Surge instances with one instance using its two scenes and separate scene outputs. Surge documents these facilities; equivalence of independent CHAOS effects, articulation, coupling, and routing must be established. Shared/global processing may change the result, and savings are unknown. Retain two instances if required capabilities or measured efficiency favor them. [Surge scenes and outputs](https://surge-synthesizer.github.io/manual-xt/#scenes)
7. **Make time behavior explicit.** Convert block-dependent smoothing/random-update behavior to time-based behavior for the new engine, with legacy semantics available where patch migration needs them. Reassess numeric precision, denormals, repeated safety scans, and nonlinear feedback quality through tests. Avoid global fast-math or lower quality as an undocumented performance shortcut.

For equivalent optimizations use controlled/toleranced render comparisons, state/transition tests, and actual performance results. For intentional redesign use versioned behavior, listening comparisons, and measured quality/CPU tradeoffs. CONTOUR and drone remain separate feature experiments; no automatic ECO/FREEZE product mode is required to do the above work.

### New measurement contract

Create fresh workloads independent of the old build: routing/mixing, dense modulation, event bursts, one optimized CHAOS, multiple independent instruments, heavy effects, and asynchronous sensors. Record quality, sample rate, render quantum, device latency, polyphony, and actual enabled processing. Avoid making three CHAOS instances the singular goal.

Measure CPU-seconds per processed audio-second, full processing/callback p50/p95/p99/p99.9/max, actual deadline misses and device flags, allocations/copies, memory, cold startup/JIT cost, steady-state capacity, and UI-on versus headless behavior. Record sustained thermal/power behavior where measurable. Separate untraced timing from allocation diagnostics.

Use 48 kHz/512 only as one legacy comparison setting. Compare new 128/256/512 configurations and additional rates where supported, without hiding buffering latency. Compare different tools on matched tasks, quality, and latency before making superiority claims. Strong music software supplies useful reference cases; it does not set the ceiling.

Acceptance has two levels: a small reliable release workload with generous measured headroom, and a scaling frontier showing how far capability can grow at fixed latency/quality. A p99 target alone is insufficient: inspect worst cases, actual device underflows, burst behavior, and maximum supported patch complexity. Each optimization should improve a whole-pipeline result enough to justify its maintenance cost.

## Implementation sequence and acceptance gates

These are implementation milestones to begin only after the plan is accepted. Each ends with a runnable checkpoint, a clear source diff, updated compatibility evidence, and a rollback point.

### Milestone 0 — Preserve the old build; define the new targets

**Deliver:** source control and immutable legacy snapshot; preserved patches/presets and Surge binary; canonical import/version inventory; historical-format fixtures and representative controlled renders. Document Windows/Pi deployment assumptions and define new functional, quality, latency, efficiency, and scaling workloads before comparing implementations. Seed portable CI and separate tests requiring native assets/hardware.

**Gate:** reproduce the old correctness baseline where possible, establish recovery, and record provenance. No old CPU number becomes a cap or promise for the new system. Keep development isolated from the working installation.

### Milestone 1 — Prove native deployment and processing choices

**Deliver:** a minimal standalone Qt window with representative Studio controls and dense-canvas pan/zoom; a headless native runner with a simple oscillator/gain path; a pinned direct-Surge integration proof; three equivalent numerical kernels plus graph/event workloads for CPython, PyPy/CFFI, Numba/Cython, and native comparison as applicable. Use the new measurement contract.

**Gate:** compare total CPU, timing tails, startup, quality, and packaging. Show that GUI activity cannot block engine processing through Python/Qt calls. Select a primary engine/kernel toolchain and GUI facade from evidence; do not fully port every candidate. Confirm Windows packaging and an early ARM64 build/deployment smoke test. Physical Pi GUI-plus-audio testing is an explicit hardware gate; if the device is unavailable, retain a clearly provisional Pi deployment decision.

The recommended default remains native C++ processing plus Qt Quick. A compiled-Python component can win its kernel; PyPy can win suitable control/data algorithms. This milestone validates the architecture before a broad rewrite or public SDK commitment.

### Milestone 2 — Build document safety and the minimal module contract

**Deliver:** GUI-independent document controller; explicit format readers/migrations; missing-module placeholders; atomic save/recovery; typed revisioned in-process commands; transactional registry; pure validation; draft native processor interface and Python binding/tier contract. Include stable IDs, parameters, events, timebases, lifecycle, buffer ownership, and generic native module UI.

**Gate:** `wirescape.patch` v1/v2/v3 and `wirescape.visual.study` v4 fixtures migrate by `(format, version)`; unknown state/cables survive; unsupported future documents stay intact; failed module load cannot corrupt the registry; UI lifecycle cannot destroy live DSP objects. Port data meaning before optional algorithm redesign.

### Milestone 3 — Prove the full core through three module paths

Build these paths using the selected execution model and draft SDK:

1. Independent LFO → generic GAIN parameter → output, with a deterministic source.
2. Migrated sequencer → CHAOS native/compiled processor → output.
3. Simulated asynchronous sensor → range/smoothing adapter → the same parameter API.

**Deliver:** prepared schedule/buffers, bounded command classes, shared transport/events, parameter precedence, safe activation/retirement, initial analysis snapshots, and timing/latency diagnostics. Run the same patches with and without the GUI. Prototype control-only operation with audio off.

**Gate:** modules install/load/save/process/stop/dispose without host edits; sample/event tests and note ownership pass under bursts; no unbounded processing-side imports/allocations/lifecycle work; compare fresh capacity and whole-pipeline CPU/tails. Render comparisons distinguish equivalent optimization from versioned musical change and control stochastic state where possible.

### Milestone 4 — Optimize CHAOS and finish the native application

**Deliver:** unified final parameter cache, dirty dependencies, ring-buffer/fused-kernel changes supported by measurement, presentation-free processing, and native CHAOS/sequencer editors. Complete MODULATOR, GAIN, AUDIO I/O, CHAOS, and SEQUENCER migration. Evaluate the one-instance/two-scene CHAOS experiment separately before deciding its engine arrangement.

**Gate:** patch/preset data, keyboard targeting, articulation, RAW ownership, loop capture/playback, tails, and routing transitions behave as specified; improvements show useful end-to-end results at matched quality/latency. Changing render quantum does not accidentally change modulation speeds. Do not wait for every speculative optimization before shipping a proven foundation.

### Milestone 5 — Devices, independent SDK proof, and release

**Deliver:** MIDI provider/Learn/Monitor, one OSC loopback provider, shared clock/reset/loss semantics, reliable module manager/diagnostics, supported audio-channel/rate configuration, user-state migration, standalone installers/packages, and current SDK/docs. Validate the first real Pico transport and M8 workflow as hardware becomes available.

**SDK gate:** an independently authored audio plugin, event plugin, and async provider install with **zero core-source changes**, preserve their state, and clean up correctly. Publish SDK v2 only now, including native templates and the compiled-Python recipes validated in the experiments, generic/custom UI guidance, API-v1 migration guidance, and a validation command. Runtime-backed/JIT tiers declare their dependencies and remain experimental until their loading/lifetime/packaging gates pass. Keep directory organization secondary to stable identity and metadata.

**Windows gate:** clean standalone installation, native GUI interaction tests, repeated load/remove/failure cycles, offline correctness, and sustained user-initiated actual-device audio under controls/UI/data load. Report the full new metrics and capability frontier. A 20-minute stability session is a minimum checkpoint rather than the whole reliability case.

**Pi gate:** native application and headless engine on real ARM64 hardware, supported display/graphics stack, native Surge build, USB audio/device tests, sustained temperature/power observations, and independent performance results. Windows support can ship first, but Pi feasibility must have been checked early rather than postponed until the architecture is fixed.

**Foundation ready means:** standalone app, protected old work, extensible module contracts, coherent timing/parameters/devices, bounded processor execution, measured efficiency, documented supported workloads, and a proven deployment path. Advanced musical modules remain follow-on work.

## What follows the foundation

1. **Essentials:** VCO/noise, ADSR/VCA/mixer, scale/offset/attenuverter, converters, clock/logic/quantizer, envelope follower. Add them as ordinary independent modules, using already-proven contracts.
2. **Instrument features:** Quad VCA/LPG; sequencer refinements; advanced multichannel/shapeable modulation. CHAOS CONTOUR and an independent drone path require focused Surge feasibility tests, particularly polyphonic articulation and routing cost. Preserve meaningful existing presets through explicit engine/state versions while allowing improved algorithms.
3. **Physical workflows:** real M8 clock/notes/CC, chosen Pico transport, calibration profiles, and actual Focusrite/Behringer/laptop-input tests. Select exact models/firmware during implementation testing.
4. **Pi 5 appliance refinement:** after the early feasibility and physical release gates, optimize startup, fullscreen/touch or controller operation, packaging, and supported patch capacities. Use the same portable core and SDK; make no capacity claim before actual measurement.
5. **Visual/data/lighting:** add real workload-specific runtimes and resource ownership when needed. Keep typed async messages and provider hooks now; defer video/GPU APIs, DMX stacks, full DAW plugin hosting, parallel DSP, and general latency compensation.

CHAOS ECO/FREEZE product modes remain outside the current plan. Core and module efficiency work is fully in scope. The native interface preserves the Studio identity; optional Python/compiler tools do not become a mandatory new DSP language for every module author.

## First implementation scope

The first coding checkpoint should be **Milestone 0 plus the focused feasibility experiments in Milestone 1**. Its reviewable result is a protected old build, fresh performance criteria, a standalone GUI/engine proof, and evidence selecting the main toolchain. The full application/SDK build follows those results. This review has created only this planning document; implementation has not started.

## October 26 addition: hardware reuse goal

Treat reusable C++ DSP and thin host/board adapters as an architectural requirement. A future third-party agent should be able to statically assemble a saved patch into target firmware without rewriting supported module algorithms. Wirescape itself need not export/build firmware. See EMBEDDED_PORTABILITY.md for honest current desktop-host limits, resource/dependency declarations and required patch handoff semantics. Do not claim Daisy/MCU support before cross-build and hardware measurements; do not constrain desktop-only modules to a board profile.
