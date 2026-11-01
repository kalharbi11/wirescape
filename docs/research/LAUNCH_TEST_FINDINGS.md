<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Launch-test findings — source-grounded issue routing

**Audit date:** 2026-11-01
**Baseline:** main at 6e3c253550428f2bb964af4434bd429b5284cce2 (read-only)
**Source:** [owner field report #182](https://github.com/kalharbi11/wirescape/issues/182) and [Jam checkpoint #45](https://github.com/kalharbi11/wirescape/issues/45)
**Input limitation:** The request's OWNER NOTES section contains literally <paste here>. No new notes, screenshots, timestamps, patch files, or HTML sketch were attached for this audit. The eight routed categories below normalize **prior** owner observations in #182; they are **not** eight newly reported defects. Do not open new issues from this document without comparing additional notes with the existing records.

**Method:** Read AGENTS.md first, the Now portion of docs/PLAN.md, #45, #182 and its planner triage comment, relevant existing issues and task acceptance blocks, and narrowly targeted production sources. Static source-path verification is **not** device reproduction, a passing test, proof of root cause, or a successful Windows/Pi launch. No code, devices, LAN endpoints, tests, labels on other PRs, or reviews were changed or run. No issues were created.

## Executive routing and checkpoint assessment

| Finding / owner observation | Existing work (do not duplicate) | Primary classification | Severity | Blocks #45? | Evidence boundary |
| --- | --- | --- | --- | --- | --- |
| F01. Cannot drag modules or author/disconnect cables at 85% / 62% zoom | [#183](https://github.com/kalharbi11/wirescape/issues/183); related [#141](https://github.com/kalharbi11/wirescape/issues/141), [#178](https://github.com/kalharbi11/wirescape/issues/178) | Qt-specific gesture/hit-test path; document controller remains needed | **blocker** | **Yes** — patch from scratch and LFO routing are #45 steps | Owner observation; mouse event code path verified, failure not reproduced here |
| F02. Launchkey appears in MIDI panel but Received = 0 / no notes reach synth | [#184](https://github.com/kalharbi11/wirescape/issues/184); related [#163](https://github.com/kalharbi11/wirescape/issues/163) | Engine/runtime + device backend, with Qt settings front end | **blocker** | **Yes** — USB notes, sustain, reconnect | Physical endpoint identity/traffic still unverified |
| F03. Audio starts visually but output selection or audibility unclear | [#140](https://github.com/kalharbi11/wirescape/issues/140) | Runtime/device layer + Qt selector | **blocker** if real output silent; otherwise major | **Yes if no audible output** | Code proves stereo-only selection, not the cause of owner silence |
| F04. While playing, modules/cables are locked; Start/Stop and keyboard focus interrupt play | [#186](https://github.com/kalharbi11/wirescape/issues/186), [#201](https://github.com/kalharbi11/wirescape/issues/201) | Qt gating plus engine graph swap policy; separate Qt keyboard-focus regression | **major** for live edits; **blocker** if silence prevents play | **Conditional**: audible/start/key input essential; full topology hot-edit is not explicitly an #45 step | Static lockout reproduced from conditionals; sound/focus owner retest required |
| F05. Raw VCO tooltip precision; desired LFO phase / effective-value dots | [#185](https://github.com/kalharbi11/wirescape/issues/185) (closed), [#143](https://github.com/kalharbi11/wirescape/issues/143), design [#177](https://github.com/kalharbi11/wirescape/issues/177) | Tooltip and drawing Qt-specific; truthful telemetry remains engine/runtime | **minor** tooltip; **major** telemetry visibility | **No** for cosmetic markers; **Yes** if actual LFO-to-cutoff signal fails | Formatter present on current main; telemetry and audible modulation not proven |
| F06. Starter patches/library missing; event cables in MIDI starter documents reject | [#187](https://github.com/kalharbi11/wirescape/issues/187), [#198](https://github.com/kalharbi11/wirescape/issues/198) | Library UX design; event-identity bridge/runtime persists | **major** library; **blocker** for event starter paths that cannot load | **Conditional** — required MIDI patch/load must work, library convenience does not gate | #198 has a concrete event-identity diagnosis; no test run here |
| F07. Manual Qt environment, PortAudio packaging ambiguity, stale shortcuts | [#188](https://github.com/kalharbi11/wirescape/issues/188); related [#154](https://github.com/kalharbi11/wirescape/issues/154) | Packaging/launcher | **major** | **Conditional** — must launch app for #45; shortcut and updater not gates | Existing dev launcher and package guards identified; owner's actual DLL and shortcuts not inspected |
| F08. Simple Check for updates, long-term Output-module hardware routing, per-device MIDI mapping | Existing [#188](https://github.com/kalharbi11/wirescape/issues/188), [#140](https://github.com/kalharbi11/wirescape/issues/140), [#163](https://github.com/kalharbi11/wirescape/issues/163); future design, no new issue yet | Product design / packaging | **minor** to **major** depending chosen scope | **No**, absent a redefinition of #45 | Owner-requested direction, not evidence of a failing current implementation |

**Counts:** 0 new owner findings provided; 8 existing-report categories normalized; 0 issues created; 0 physical-device reproductions; 0 test executions. Existing GitHub issues are the action queue; these are **review-ready specifications / routing notes, not tickets to file again**. Source paths below are anchored to the baseline SHA above, not to unmerged PRs.

## F01 — Canvas and cable interaction (reuse #183)

**Fact (owner):** Cannot move modules, create/remove/repatch cables at observed zoom levels; #182 explicitly warns that a prior successful geometry change (#141) is not functional editing proof.

**Static reproduction path:** Qt QML ModuleCard header uses MouseArea drag.target and commits move on release (native/ui/qml/ModuleCard.qml:22-29). Output port press stores a pending wire, and release calls completeCableDrag after a drag threshold (native/ui/qml/ModuleCard.qml:79-88); input-port mouse click invokes chooseInput (native/ui/qml/ModuleCard.qml:58-68). Main.qml completes the wire from pending source to a compatible input and maps transformed coordinates into module card hit regions (native/ui/qml/Main.qml:103-121; native/ui/qml/ModuleCard.qml:43-56). The workspace pans/zooms under a separate MouseArea and world transform (native/ui/qml/Main.qml:345-357). Controller connection validation then checks native descriptor port kinds before creating a document edge (native/ui/studio_controller.cpp:342-357). **No synthetic mouse events were executed in this audit**, so the exact failure trigger remains an investigation rather than a proven gesture defect.

**Migration:** Qt-specific event handling and geometry disappear if the QML canvas is deleted; the ability to connect correct typed ports, persist stable IDs and reject bad edges remains. An HTML sketch implementing gestures is not evidence that host-side patch acceptance or runtime wiring works.

**Issue draft disposition:** Already filed as #183; retain issue number, severity **blocker**, **blocks #45: yes**.

**Title:** P0: restore drag/move/connect/disconnect in Studio at multiple zoom levels  
**Problem:** Owner cannot author the graph needed to test MIDI, LFO, and the Jam patch.  
**Expected:** Dragged module positions and compatible cables persist, hit testing works under pan/zoom, and incompatible edges fail without modifying the accepted patch.  
**Suggested agent:** Codex — actual Qt event tests, controller fixes and CI build evidence are required.

```acceptance
tier: T1
size: <= 300 changed lines, <= 6 files
must_pass:
  - ci: windows-x64 studio_ui_tests: actual press/move/release for module move, VCO -> VCA -> Output, disconnect/reconnect, LFO -> CV at 100%, 85%, 62% zoom
  - ci: save/reload preserves the same stable port/node IDs and edges
  - ci: linux-arm64, sanitizers, guard green on exact head
  - user: owner repeats failed gesture with the same patch/zoom and confirms response
stops: [patch format or stable ID change, device access in tests, new dependency]
```

**Do not split into:** A new cable-geometry ticket; #141 is closed and only concerned shape/layout. Native event-cable identity is distinct and tracked by #198; modulation-to-parameter gestures require the approved #177 design, not an invented signal-port shortcut.

## F02 — MIDI enablement, endpoint identity and global keyboard (reuse #184)

**Fact (owner):** The Launchkey presents two named ports; the selected MIDIN2 port showed Received 0 and no instrument response. The owner's primary note port has not been independently proved. Do **not** label the MIDI backend or synth DSP broken on this snapshot alone.

**Static reproduction path:** Studio has a user-facing MIDI authorization switch, per-endpoint selection and per-port traffic presentation (native/ui/qml/Main.qml:165-187). Authorization is saved through QSettings only where interactive MIDI preferences exist; it refuses mutation during an active audio stream (native/ui/studio_controller.cpp:591-624). Application startup chooses interactive vs probe mode and parses an explicit --confirm-midi option (native/ui/main.cpp:30-43; native/ui/main.cpp:53-57). The service subscribes enabled endpoints and a host.keyboard global input route; midi.in has a separate route (native/control/midi_host.cpp:74-97). Native UMP note events are decoded into note-on/off for that route (native/control/midi_host.cpp:42-64), while diagnostics emit per-port counters (native/control/midi_host.cpp:108-124). Thus the problem crosses the web UI boundary: replacing QML would not establish correct device permission, routing, note-off or reconnect behavior.

**Reproduction request (owner only):** With audio deliberately enabled on a known good patch, compare the Launchkey primary input and MIDIN2 input, press/release individual keys, record received/dropped counters and audible/visible results, unplug/reconnect, then test sustain and panic. Agents and CI must use a fake backend and never open physical MIDI.

**Issue draft disposition:** Already #184; **blocker**, **blocks #45: yes**. More advanced device/channel mapping is already #163, not a second default-keyboard issue.

**Title:** P0: Launchkey notes must reach the patch with explicit remembered interactive MIDI consent  
**Problem:** Endpoint enumeration is not end-to-end note delivery, and port labels can obscure the note-carrying input.  
**Expected:** Distinct endpoint names/system IDs with independent counters, a saved interactive-enable preference, a sensible global keyboard route and safe disconnect/panic.  
**Suggested agent:** Codex — backend, controller, threading and Windows/Pi CI coverage; T2 requires independent review.

```acceptance
tier: T2
size: <= 900 changed lines, <= 17 files
must_pass:
  - ctest --test-dir build/native -R "^(abi2_midi_service_tests|midi_host_tests)$" --output-on-failure --no-tests=error
  - ci: fake-backend per-endpoint counts, UMP note-on/off, default host.keyboard route, saved consent, panic and unplug/replug release all notes
  - ci: windows-x64, linux-arm64, sanitizers, guard green
  - user: Launchkey primary and secondary ports differentiated; note/sustain/unplug/replug audible on Windows, then Pi
stops: [ABI edit, ID change, device access in tests, unapproved dependency]
```

## F03 — Physical audio selection and silent stream (reuse #140)

**Fact (owner):** UI/device discovery improved, but reported green launch / active audio was not evidence of audible output. Exact device backend, channel capability, and live route were not attached.

**Static reproduction path:** AudioDevice::devices adds a system-default entry, then **omits devices with maxOutputChannels < 2** (native/ui/audio_device.cpp:85-96). startRender also refuses a non-stereo output and requests a stereo float32 stream (native/ui/audio_device.cpp:120-141). Refresh is rejected while a stream or engine owner remains (native/ui/audio_device.cpp:66-76; native/ui/studio_controller.cpp:378-379). Studio start compiles the patch, opens the stream at 48,000 Hz and starts MIDI only afterward (native/ui/studio_controller.cpp:381-398). The separate graph Output button selects a patch output node (native/ui/qml/ModuleCard.qml:141-147), not a PortAudio device. A running flag or a readiness report does not prove sound at speakers.

**Migration:** PortAudio enumeration, stream ownership, stereo policy, underrun/error reporting and actual audio routing **stay**. QML combo and refresh widgets are replaceable. Moving hardware selection onto each Output module would be a separate design expansion.

**Issue draft disposition:** Already #140; **blocker if playback is silent**, **blocks #45 if so**; otherwise major UX work. Do not assert which driver/device caused the field observation.

**Title:** Diagnose real audible output and expose selected physical audio device/state  
**Problem:** Users cannot distinguish silent graph, unavailable interface, unsupported output channels and running-but-inaudible device selection.  
**Expected:** Discoverable/diagnosable output selection, explicit stream state, recoverable stop/cleanup/refresh/start and a truthful audibility checklist.  
**Suggested agent:** Codex — native callback ownership, fake PortAudio tests and Windows CI evidence are required.

```acceptance
tier: T2
size: <= 600 changed lines, <= 10 files
must_pass:
  - ctest --test-dir build/native -R "studio_ui_tests|controller_failure_tests|audio_device_tests" --output-on-failure --no-tests=error
  - ci: fake stereo/mono/default/disconnected device enumeration; output selection; recoverable cleanup and explicit failure diagnostics
  - ci: Windows Qt and Linux/sanitizer tests green on exact head
  - user: select actual interface, start, hear expected patch, document rate/channels/route on laptop and Pi
stops: [ABI edit, new backend/dependency, real devices in tests, softened thresholds]
```

## F04 — Running graph edit lockout and transport focus (reuse #186 + #201)

**Fact (owner):** Add/connect actions were unavailable while running; audio felt dependent on repeated Start/Stop and touching modules.

**Static reproduction:** Main.qml explicitly blocks chooseOutput and completing a pending cable during playback (native/ui/qml/Main.qml:103-115); the module library and several toolbar actions are disabled while running (native/ui/qml/Main.qml:257-278; native/ui/qml/Main.qml:303-314), as are disconnect and output-node select (native/ui/qml/Main.qml:422-425; native/ui/qml/ModuleCard.qml:141-147). The controller's commitTopology refuses changes while audio is running or a stream still owns the engine (native/ui/studio_controller.cpp:297-307). These conditions **prove the editing restriction from source**, but do not prove the owner's silence is caused by focus, note gate, routing or physical output. Keyboard note input only handles keys when the keyboard surface owns active focus (native/ui/qml/Main.qml:97-101; native/ui/qml/Main.qml:206-228). The transport button invokes focus restoration after start/stop (native/ui/qml/Main.qml:307-315); its regression test is specifically tracked by #201.

**Migration:** The QML lockout and keyboard focus model disappear in a web UI, but hot-swapping a running native graph safely is an engine/runtime design and synchronization problem. Qt removal must not encourage allocation, mutexes, logging or deletion on the audio callback.

**Issue draft disposition:** #186 covers live topology; #201 separately covers the independently actionable focus bug. **Severity major** for live edit; **blocker only when audio, keyboard note delivery or audible response prevents the #45 scripted session**. #45 does not explicitly require connecting new cables during playback: do not expand the checkpoint gate without planner consent.

**Title:** Permit safe live topology edits; keep transport keyboard playable  
**Problem:** Current controller disallows structural edits while the stream owns the engine; focus/start behavior may interrupt keyboard performance.  
**Expected:** Safe staged graph application while rendering, with bounded interruption and no callback allocations; independent transport-focus restoration.  
**Suggested agent:** Codex — engine sequencing and concurrency tests for #186; same agent for narrow QML #201 regression.

```acceptance
tier: T2
size: <= 700 changed lines, <= 10 files
must_pass:
  - ci: fake/offline rendering continues through valid add/move/connect/disconnect without lost accepted connections
  - ci: callback allocation/locking guards and sanitizer concurrency checks stay green; preserve correct note-off/panic on graph change
  - ci: windows-x64, linux-arm64, sanitizers, guard green
  - user: while audio plays on Windows, patch live and verify no stuck notes/unsafe device transition
stops: [ABI/patch schema/ID change, threshold change, physical devices in CI]
```

**#201 separate acceptance:** tier T1, <=150 changed lines/3 files, Windows studio_ui_tests transportStartReturnsKeyboardFocus using a fake Studio and real Main.qml; Linux/sanitizers/guard. Do not fold #201 back into a broad graph-swap branch.

## F05 — Tooltip formatting vs truthful modulation and LFO phase (reuse #185, #143, #177)

**Fact (owner):** One VCO tooltip showed binary float noise; knob gesture/appearance was liked. Desired dots are two different quantities: (1) actual oscillator phase/activity and (2) effective modulation of an eligible parameter. The second was not testable during the cable failure.

**Static source assessment:** Current ParameterControl formats bounds to three significant digits, converts large Hz values to kHz and fractional seconds to ms, then uses that string for the knob tooltip (native/ui/qml/ParameterControl.qml:20-35; native/ui/qml/ParameterControl.qml:87-101). That is consistent with closed #185, **not proof of a Windows owner retest**. QML currently binds displayed knob position/value to accepted parameter values (native/ui/qml/ParameterControl.qml:14-19; native/ui/qml/ParameterControl.qml:89-100); drawing an independent animated dot here would not establish an actual DSP phase or measured post-modulation value. The approved separate parameter-edge design identifies stable target parameter IDs and host-side normalized modulation without modifying base values (docs/design/M3_PARAMETER_EDGES.md:9-27; docs/design/M3_PARAMETER_EDGES.md:86-106). #143 requires bounded, render-owned telemetry and correctly distinguishes phase from effective parameter modulation.

**Migration:** Tooltip and dot drawing are Qt-specific (replaceable). DSP phase/effective-value sampling, bounded telemetry and mapping remain engine/runtime requirements. The HTML sketch's motion is not a valid substitute for DSP-origin measurements.

**Issue draft disposition:** No new tooltip issue (#185 closed). #143 is the **major** telemetry task; #177 is approved design only and not its implementation. Cosmetic dots **do not block #45**; a functionally disconnected LFO-to-filter route **does block** the checkpoint and should attach to #183/#198 or a specific proven runtime edge defect rather than a dot ticket.

**Title:** Show real LFO phase and effective modulated value, preserving base knob value  
**Problem:** An animation based on wall-clock UI time could misrepresent stopped, disconnected or clamped DSP.  
**Expected:** Bounded source-of-truth snapshots from render owner; hidden/frozen indicators whenever no valid telemetry; no audio-thread UI calls.  
**Suggested agent:** Codex — T2 engine telemetry and race-safe tests with Qt presentation, independent review.

```acceptance
tier: T2
size: <= 700 changed lines, <= 12 files
must_pass:
  - ctest --test-dir build/native -R "studio_ui_tests|controller_failure_tests|audio_device_tests" --output-on-failure --no-tests=error
  - ci: deterministic fake/offline snapshots cover positive/bipolar/clamped, paused/unpatched/disconnected, unchanged base parameter and finite bounded values
  - ci: callback allocation and race checks; Windows offscreen named visual tests; Pi evidence for callback instrumentation
stops: [ABI edit, ID/format change, new dependency, invented telemetry]
```

## F06 — Starter patches, patch library and native event cables (reuse #187 + #198)

**Fact (owner):** Requested more guided examples and an in-app recent/favorites/open library; #187 already tracks it. Existing demo documentation says Open loads Six_Essentials_Demo.json with audio off (native/examples/README.md:1-5). Current Main.qml has a generic JSON Open FileDialog but no saved-patch library in that block (native/ui/qml/Main.qml:190-200; native/ui/qml/Main.qml:307-311).

**Code-path qualification:** The controller validates cable kind audio/control/event and matches against descriptor port kind (native/ui/studio_controller.cpp:212-219; native/ui/studio_controller.cpp:238-258); it exports module port kind through the catalog metadata path (native/ui/studio_controller.cpp:145-160). #198 records a more specific *native event-identity translation* issue in v2 MIDI starter documents. Do not mistake a JSON example that opens in a generic editor for a rendered/connected MIDI chain. The current native/examples folder has three JSON patches (Six_Essentials_Demo.json, chaos_two.json, chaos_usb_midi.json) in this audit; this is an inventory count, not a quality or coverage verdict.

**Migration:** Preset browser UI is Qt-specific and can be rebuilt in HTML. Stable event identities, round-trip patch preservation and rendering remain essential regardless of frontend.

**Issue draft disposition:** #187 library **major**, not by itself a #45 blocker; #198 **blocker if MIDI patch event cables cannot load**, conditionally blocks #45. #187 is marked blocked and #198 is in progress: sequence before assuming presets pass. Do not create a second event-cable issue.

**Title:** Guided starter patch browser with native event-safe load/save  
**Problem:** Discoverability is limited, and MIDI starter patches require correctly preserved event identities.  
**Expected:** Examples with prerequisites and expected output; recent/favorites without private-file overwrite; actual native event endpoints survive load/edit/save/reload.  
**Suggested agent:** ChatGPT for bounded QML/examples/docs #187 (CI builds it); Codex for core metadata/identity #198 (separate owner files and runtime tests).

```acceptance
tier: T1
size: <= 500 changed lines, <= 12 files
must_pass:
  - ci: every bundled patch loads and renders the declared expected output offline without hardware access
  - ci: windows-x64 studio_ui_tests for bundled/recent/favorite/open flows; no writes to arbitrary user patches
  - ci: #198 controller_failure_tests preserve event kinds and stable identities through save/reload, reject mixed kinds transactionally
  - ci: linux-arm64, sanitizers, guard green
stops: [ABI/ID/format change, private user data scanning, real device access in tests]
```

**Split contract:** #198 independently uses tier T1, <=400 lines/6 files, as in docs/tasks/J_STUDIO_EVENT_CABLES.md. Keep the core facade change separate from UI patch-library work.

## F07 — Runtime dependency selection, packaging and shortcuts (reuse #188)

**Fact (owner):** A direct executable invocation initially returned to the shell; a launch with manually set Qt runtime paths worked. A guessed PortAudio DLL path failed package preconditions and was not verified. Neither a real-DLL package pass nor migrated shortcuts have owner evidence.

**Static source assessment:** The dev launcher constructs Qt plugin/QML environment variables (tools/dev.py:404-418; tools/dev.py:594-613). Windows runtime selection has explicit, remembered, environment, build and repository candidates for Qt and PortAudio (tools/dev.py:506-523), validated against required files (tools/foundation/launcher_runtime.py:12-24; tools/foundation/launcher_runtime.py:35-58). Packaging refuses when executable, windeployqt or actual PortAudio file is absent (tools/dev.py:715-741). The shortcut script resolves only a verified package under the active checkout and writes Desktop/Programs links (tools/foundation/launcher_runtime.py:61-87; tools/install_windows_shortcuts.ps1:4-36). These current safeguards must be checked on the owner's actual Windows environment; they do not retroactively prove the field failure fixed.

**Migration:** Qt SDK/QML plugin deployment disappears *if* Qt is removed; packaging a webview frontend, native engine host, PortAudio and platform permissions still needs provenance, launch path, runtime-dependency integrity and correct shortcut targets. Do not interpret Tauri as automatically solving PortAudio or device permission.

**Issue draft disposition:** #188 **major**, checkpoint **conditional** (a working app launch is necessary; the desktop shortcut and updater are not explicit #45 requirements). Avoid a new update/launcher duplicate. #154 is the existing development-launch freshness track.

**Title:** Reliable Windows launch/package from the active checkout without manual Qt environment  
**Problem:** Old and new development installs can lead to wrong binaries, dependency paths and stale desktop shortcuts.  
**Expected:** Validated/visible dependency provenance, reusable user choice, runnable verified package/shortcut targeting the active checkout, no data overwrite.  
**Suggested agent:** Codex — Windows packaging, provenance unit tests and CI are needed.

```acceptance
tier: T1
size: <= 750 changed lines, <= 8 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p 'test_launcher*.py'
  - ci: fake-tree valid/invalid Qt and PortAudio, override/remembered choice, executable digest and checkout ownership
  - ci: windows-x64 green on exact head
  - user: test with the actual PortAudio DLL, verify desktop and Start Menu target and interactive MIDI consent; keep historical copy unchanged
stops: [new dependency, protected path change, tests opening devices, unverified DLL]
```

## F08 — Update entry, hardware Output routing and advanced MIDI mapping (reuse #188/#140/#163; design not a bug)

**Facts:** The owner requested a simple update affordance now, with a more complete updater later, and hardware route controls eventually associated with Output modules. The existing CLI updates command compares a recorded build SHA with remote origin/main without pulling or installing (tools/dev.py:538-575); the Qt toolbar's documented actions are MIDI, New, Open, Save, render and audio transport (native/ui/qml/Main.qml:307-315). The graph Output module selects outputNode, distinct from the PortAudio physical device selector (native/ui/qml/ModuleCard.qml:141-147; native/ui/qml/Main.qml:440-454; native/ui/studio_controller.cpp:378-398).

**Disposition:** Track the simple Update entry under #188; hardware output selection clarity under #140; per-endpoint/per-channel musical routing under #163. Full installer, multi-device routing and Output-to-hardware-bus mapping require planner design/spec before new implementation drafts. **Severity minor/major feature requests, not current #45 gates.** Never conflate an owner preference with reproduced broken behavior.

**If the planner authorizes a later separate issue, draft title:** Design explicit graph Output-to-physical-bus assignment with safe patch round-trip.  
**Problem:** A graph output node and a physical device stream are distinct concepts; per-output routing is not yet specified.  
**Expected:** Chosen backend/bus identity, missing-device behavior, global/per-patch policy, no silent remapping, preservation of unknown saved properties, desktop/Pi target design.  
**Suggested agent:** Claude planner for specification and acceptance review before Codex runtime implementation.

```acceptance
tier: T2
size: <= 250 documentation lines, 1 new design file
must_pass:
  - planner: approve stable-ID/patch-compatibility implications and default single-output behavior
  - review: enumerate offline fake-device scenarios, disconnect/fallback and user-only physical tests
  - docs: no engine, SDK, GUI or patch-format implementation in design PR
stops: [unreviewed ABI/patch representation change, new backend, automatic device activation]
```

This F08 block is **design-only contingent scope**, not a new ticket to file during the pause.

## Web UI migration impact: recommended boundary

**Facts about current implementation:** Main Qt entry loads qrc:/qml/Main.qml via QQmlApplicationEngine (native/ui/main.cpp:53-63). Graph-gesture/canvas and parameter presentations are QML (native/ui/qml/Main.qml:345-399; native/ui/qml/ParameterControl.qml:87-101). The existing controller compiles/validates topology and forwards runtime commands (native/ui/studio_controller.cpp:212-258; native/ui/studio_controller.cpp:289-307; native/ui/studio_controller.cpp:342-360). MIDI service routes are Qt-free native control code (native/control/midi_host.cpp:42-64; native/control/midi_host.cpp:74-97), and audio streams are native PortAudio code (native/ui/audio_device.cpp:120-141).

**Recommendations (not implementation findings):**
1. Treat browser gestures and QML look/feel failures as *replaceable implementations*, not proof that a web frontend is automatically correct. Preserve typed cable kind, stable IDs, lossless patches, and rejected-edit rollback as frontend-independent behavior.
2. Specify a narrow asynchronous command/ack and bounded telemetry interface between web UI and native engine host before selecting Tauri/WebView/localhost transport. No callbacks from the real-time render thread into web JS or blocking IPC. Keep physical devices/LAN disabled in all automated tests.
3. Compare the owner's single-file HTML sketch only when it is attached; it was not part of this input. Do not infer its feature completeness or architecture from the description alone.
4. Do not make a migration decision by counting Qt-specific bugs. Separate the UI redo cost from persistence, engine-swap, native audio/MIDI, licensing and Windows/Pi packaging costs.

## Dedupe map and missing evidence

**Existing routing:** Parent #182 contains the owner observations, with planner triage to #183 (canvas), #184 (MIDI), #185 (tooltip, closed), #186 (live edits), #187 (library), #188 (launcher/update), #140 (audio output), #143 (telemetry), #163 (advanced MIDI), #177 (parameter-edge design), #198 (native event-cable metadata) and #201 (transport focus). #141 is prior closed visual geometry work; #178 covers stereo-paired ports only. Do not edit these issues or interpret closed tickets as owner-passed #45.

**Outstanding validation:** (a) actual new raw notes/screenshot files and exact date/build SHA, (b) current executable vs packaged/shortcut target, (c) starting patch and module/port IDs, (d) reproduction with audio stopped and running at 100/85/62% zoom, (e) primary vs secondary Launchkey endpoint counters and routing, (f) selected physical output/backend and real audible result, (g) MIDI event cable load/save and patch-specific path, (h) Windows and Pi owner retest. A screenshot of a running UI or CI pass is not an audible proof. Only the owner should perform physical-device checks.

**No new tickets from the provided placeholder:** If new notes arrive, attach each to one of the existing issue records above; create a new draft in this file only for a genuinely distinct symptom and name its evidence, severity, checkpoint effect, source route, agent and acceptance block. Any new patch-format/ABI/RT design needs a planner decision first.

## Decisions needed from the planner

1. **Source of launch findings:** (A) Wait for actual pasted notes/screenshots and classify them against #182; (B) treat #182 as though it were newly submitted. **Recommend A**: retain the existing report as prior evidence and prevent fabricated duplicates.
2. **#45 gate semantics:** (A) Gate on its specified patch creation, real audio, MIDI/OSC, save/reload and Pi tests; (B) also require full topology hot-edit, integrated updates and preset library. **Recommend A**, while tracking #186/#188/#187 as explicit follow-ups unless a live-edit lock blocks a scripted checkpoint step.
3. **UI replanning:** (A) First specify/test an engine-host/frontend protocol and compare Qt fix vs web-port cost using the supplied HTML sketch; (B) immediately replace Qt and assume frontend defects disappear. **Recommend A**. No migration implementation during the pause.
4. **MIDI consent and routing:** (A) explicit remembered interactive MIDI enable, with hardware-off CI and global keyboard default, then advanced mapping #163; (B) require manual CLI flag/per-port wiring forever; (C) activate physical endpoints automatically in every launch/test. **Recommend A**, consistent with #184 and device safety.
5. **Audio output roadmap:** (A) Finish #140's single real physical-output diagnosis and #45 audible retest first; (B) design multi-device buses and move selectors onto Output modules immediately. **Recommend A**; only after the base path is proven, authorize a separate reviewed design.
6. **Truthful motion:** (A) publish bounded actual LFO phase/effective parameter values under #143, with missing-telemetry indicators hidden; (B) animate a cosmetic dot in UI without DSP correspondence. **Recommend A**; fake motion would conceal rather than resolve a routing bug.
