# Wirescape follow-up review and next implementation plan

Date: October 26, 2026. Supersedes the status, not the historical evidence, in `REVIEW_2026-10-26.md`.

## Latest user direction and actual outcome

Implementation was authorized for the first review's bounded fixes. The latest request is now **review and planning only; start further fixes in another prompt**. After that request, only inspection and documentation work was performed. A previously started package check was collected; no new implementation, hardware playback, or worker was started for this follow-up review.

The user tested the updated application and reported: **“its working, the audio passes through, the meter shows movement.”** Record this as a successful user hardware smoke test. The earlier reported audio problem is no longer an established current failure. Device identity, duration, underruns, repeated teardown, and latency were not recorded, so this does not establish long-session stability or support for other devices.

The normal desktop application was also observed responding through an unrestricted process query. A restricted query failed to see it; that was an observation limitation, not evidence that the app was broken. Do not repeat the earlier incorrect “not running” conclusion.

Keep the native engine / standalone Qt Quick design, independent module folders, explicit audio start, and Essentials-first sequence. No finding here requires an architecture or language restart.

## Disposition of the previous review

| Item | Implemented changes | Verification / remaining boundary |
| --- | --- | --- |
| R01 | Typed schema checks, staged document load, preservation on malformed or unexpected compilation failure, Save/Discard/Cancel for New/Open/Close, atomic save | Controller and actual QML action tests pass. Missing module values and extension fields round-trip. Actual native file-dialog success/commit-failure paths need broader interaction coverage. |
| R02 | Editable incomplete/missing-module documents, readiness diagnostics, explicit Output selection, recovery after removing last Output | Remove/save/reload/re-add/reconnect and missing-module recovery tested offline. Topology edits still require stopped audio. |
| R03 | Targeted accepted-value signal, label/slider synchronization, rejection rollback, fixed-range guard | Real mouse/keyboard/drag tests pass without replacing slider delegates. Rejection is simulated by a bridge; not an actual full controller queue saturation test. |
| R04 | Explicit audio states, injected backend, stop/abort/close errors, late-callback gate, quiescence guard, unexpected-stop polling and retry cleanup | Mock failure/lifetime tests pass; user reports live sound and moving meter. Permanent backend failure retains callback storage/library for safety; not a guarantee of cleanup under an arbitrarily broken backend. |
| R05 | Persistent controller/QML tests, generic mixer discovery test, retained default smoke test | Four CTest suites pass. Screenshot was inspected and readable at 1400×900. Tests do not cover every real dialog/focus/device interaction. |
| R06 | Command deadlines, CTest deadlines, SPSC watchdog and join before assertions | Timeout helper test passes. Full descendant-process cleanup remains a gap described below. |
| R07 | Source fingerprint/dirty flag, build flags and binary hashes, non-recursive latest-per-source reports, explicit Qt override failure, requested/built UI distinction, build-recorded SDK runtime | Five new helper tests plus three original tests pass. Provenance still needs stronger handling of concurrent edits, non-Windows artifact layouts and cached Qt discovery. |
| R08 | Documented mono ownership/retrigger policy, distinct enqueue errors, 5 ms VCO Frequency/VCA Gain ramps, render event/span budget with silence/fault recovery | Multi-rate ramp checks, partition tests and overload tests pass. Larger setup queues, mixer smoothing and focus transitions need follow-up. No polyphony/held-note manager was added. |
| R09 | Refresh before playback, success notices, explicit short-render label, common output gain, portable Windows staging and isolated-SDK-path smoke | Local package passes. Real clean-machine testing, distribution notices/source inventory, Pi and future module trust lifecycle remain outside the completed checkpoint. Idle optimization was not claimed. |
| Mixer | Active independent module, persistent processor/graph/DLL tests, generic library discovery | User compiled/tested it successfully; local two-VCO → Mixer → Output, missing inputs, gains, unclipped sum, reset, guards and chunk tests pass. No mixer branch added to host processing. |

Historical review statements saying “mixer not integrated” or “no live audio tested” describe the earlier checkpoint only.

## Changes inspected since the first review

Inspected the changed native engine/header, VCO/VCA processors, mixer and its tests, Qt controller/device adapter/QML, CMake test wiring, Python runner/helpers/reporting tests, new output policy, package results, screenshots, and build/checkpoint/workflow changes. This is a focused review of the new native work, not a renewed audit of all legacy dependencies or private data.

Important settled behavior:

- Processor buffers remain raw. GUI/live and CLI WAV export apply the shared gain `0.2` once and clamp at the host boundary.
- Notes are mono last-on/retrigger with source/note ownership. There is no held-note fallback. Manual gate and keyboard have different note identities; latest onset owns the ADSR. Gates/notes are not smoothed.
- Frequency is a saved manual/base setting; note-on selects absolute note pitch without rewriting the knob. The knob is not an effective live-pitch readout.
- VCO Frequency and VCA Gain changes after processing begins ramp over 5 ms. Reset snaps to target and clears ramp progress. This is not blanket click-free behavior for every module parameter.
- Each render call examines at most 256 queued entries and executes at most 64 graph spans. Overload silences the block, advances time, increments `overload_blocks`, invalidates queued commands and returns `WS_CAPACITY`. The device stops on a render fault and never restarts automatically.
- ABI 1 is still experimental. The meter layout was extended; old host binaries must not be mixed with rebuilt experimental engine headers/libraries.

## Current findings, in priority order

### N01 — P2: normal-launch success is not a health check

`tools/dev.py:389` starts a process and immediately records PASSED, without capturing a startup log, waiting for initialization, or proving a window is ready. The desktop app is currently working according to the user, so this is a confirmed diagnostic weakness, not a confirmed current crash.

**Next edit:** distinguish launch requested from application ready. Capture stderr with `QT_FORCE_STDERR_LOGGING=1`, add a bounded ready/exit observation, retain the log/exit status, and fail on early exit. A ready notification must not open audio. Use an app-supported readiness signal/report rather than judging solely from process existence.

**Acceptance:** ordinary development and portable launches report ready; forced early exit/missing Qt dependency reports failure with useful evidence; process/window inspection occurs in a context that can actually see the desktop process. Do not claim an invisible restricted query proves a crash.

### N02 — P2 before larger modules: startup setup shares the live event budget

`studio_controller.cpp:215` enqueues every initial parameter at frame zero. The engine's per-call dispatch budget is 256 even though a controller queue can hold 2048 events. `renderDemo()` also queues a gate for every node at frame zero, including processors that do not use gates. A sufficiently parameter-heavy valid patch can therefore compile and then fail at the first render. The short offline render reaches this limit sooner because its onset shares frame zero with parameter initialization. For example, a 64-node graph containing 44 ADSRs, 19 VCOs and one Output queues 195 initial parameters plus 64 initial gates, exceeding 256. Disconnected nodes are currently processed too.

This is a source-derived boundary case, **not a reproduced failure of the user's small patch**. The ordinary default patch is well below the limit. Future modules may have many more parameters, so queue capacity alone must not be advertised as render capacity.

**Next edit:** separate quiescent initialization from timed performance traffic, or explicitly validate/reject setup workloads before advertising readiness. Keep the real-time work bound. Avoid broadcasting irrelevant events when a generic capability contract is available; do not add module-ID switches. Make overload cause/counters visible outside the engine.

**Acceptance:** test below/at/above both dispatch/span limits; prepare and render a large parameter-heavy graph without an accidental startup fault, or reject it clearly before starting. Show that sustained overload stops safely and explicit restart reconstructs document parameters. Preserve ordinary partition/ramp tests. Avoid smoothing/coalescing note-offs as if they were redundant knob events.

### N03 — P2: finish the real Save → failure → retry workflow

The tests cover helper calls and failed destination opening. In QML, choosing Save accepts/closes the unsaved dialog, opens a native file dialog, and a failed `saveAndContinue()` leaves `pendingAction` intact. That pending action may then be invisible; a later ordinary successful Save can execute an old New/Open/Close action. The current test invokes the helper while the unsaved dialog remains open, so it does not establish real dialog behavior.

**Next edit:** keep a visible retry/cancel state after failure or clear the destructive continuation deliberately. Scope a continuation to the dialog transaction that created it. Add an injectable persistence boundary only if needed to test commit failure without damaging actual saved files.

**Acceptance:** real Save/Discard/Cancel, save-dialog cancellation, failure then cancel, failure then retry, and ordinary later Save never perform a surprising stale action. Previous saved bytes and current edits survive write/commit failure. Also exercise a fully available module's preparation failure to verify transactional document retention.

### N04 — P2: prove keyboard release across focus changes

`Main.qml:133` handles note-off through a FocusScope key-release path; window deactivation panics. There is no explicit held-key registry or test covering a key pressed on the canvas and released after focus moves to a slider, button or dialog inside the same active window. A child control can consume a release. This is a testable risk, not a reproduced stuck note.

**Next edit:** track exactly which physical keys started notes, reconcile releases/focus/modal transitions, and choose a simple documented stop/panic policy where needed. Preserve release delivery without stealing ordinary text/control keys. Keep mono last-note policy unless the user requests a held-note manager.

**Acceptance:** hold a note/Space, click a slider/button, open a dialog, switch windows, stop/restart, and release in each state. No stuck gate or automatic restart. Test overlapping notes/manual gate ownership with the actual controller/QML path, not just ADSR callbacks.

### N05 — P2: ordinary level controls still have different smoothing behavior

VCO Frequency/VCA Gain have tested 5 ms ramps; mixer Level A/B are direct assignments (`native/modules/mixer/module.cpp:41`), and ADSR sustain changes can jump while sustained. A mixer gain drag can click on a nonzero waveform. This was not audibly reproduced in this review and does not invalidate the mixer's arithmetic tests.

**Next edit:** define parameter-specific transition behavior for Essentials. Reuse a small native tested ramp primitive if it reduces duplication; keep linear envelope timing and note/gate behavior explicit. Decide which discontinuities are intentional. Do not introduce a large DSP framework for a few scalar ramps.

**Acceptance:** multi-rate/partition tests for ramps, repeated targets, rapid automation and reset. Preserve mixer missing-input, unclipped-sum and steady-gain behavior. Measure any added cost rather than assuming it is negligible or faster.

### N06 — P2: timeout does not establish descendant-process cleanup

`tools/foundation/helpers.py:144` uses `subprocess.run(timeout=...)`, which kills/waits for its direct child. Builds and test runners can have their own descendants. There is no process-tree cleanup test or guarantee that a timeout leaves no compiler/test/app child behind.

**Next edit:** use a platform-appropriate owned process group/job and terminate only that invocation's descendants on timeout. Keep full logs, timeout classification and finite cleanup deadlines.

**Acceptance:** a fixture spawns a child, both hang, the action times out, both are gone, log persists, and unrelated processes remain untouched. Existing direct-child exit/timeout tests continue to pass.

### N07 — P2/P3: reporting is improved but not an immutable build snapshot

Source fingerprints cover `native/` and `tools/`, not external SDKs, compiler binaries, CMake cache contents, root agreements or documentation. Build collection does not reject source edits made during compilation. Binary collection currently assumes Windows-style `build/native/<config>/*.{exe,dll}`; single-config Linux/ARM layouts will produce incomplete inventories. Runtime Qt metadata records the requested/detected SDK, which can differ from a Qt found through an existing CMake cache or system installation. The last CTest report predates the new post-ensure-build metadata refresh, so its embedded old build metadata is not proof of the exact binaries just tested; the raw test/build logs still establish the test run.

**Next edit:** record pre/post source identity and actual CMake-resolved SDK/configuration/artifact locations; detect changes during a run. Add temp/mock tests for the post-test metadata refresh. Distinguish current-checkpoint success from historical failures in summaries. Do not perform a broad build matrix just to test metadata selection.

**Acceptance:** dirty/untracked edits change identity; mid-build changes cannot be labelled one stable snapshot; cached alternate Qt and both artifact layouts are handled; successful test records identify the binaries actually tested; repeated report files remain bounded/non-recursive. Remove the duplicate harmless `failed = next(...)` assignment while touching `command_build`, not as a standalone cleanup task.

### N08 — P2/P3: deployment and device coverage remain bounded

The final local portable package passes with SDK paths removed. An initial package smoke failed because windeployqt deployed the Windows platform plugin but omitted offscreen; explicit offscreen inclusion fixed that. This was an automated-test dependency failure, not evidence that the live audio device was broken.

Remaining coverage: a genuinely clean Windows environment, normal graphics-backed launch of the portable copy, multiple output backends/devices, fixed 48 kHz/stereo compatibility, repeated start/stop/device changes, and sustained underrun/error observation. `refreshDevices()` reuses an initialized PortAudio instance; hot-plug re-enumeration and negative device-count diagnostics are not established. Backend calls themselves can block despite the callback-detach watchdog. Permanent close+terminate failure intentionally retains callback context/code until exit.

**Next edit:** first collect a small device/session diagnostic with rate/block/backend/device and counters. Add explicit format support/error handling before any multi-rate UI expansion. Test refresh/hot-plug semantics while stopped. Keep packaging a development artifact until dependency notices/source inventory and clean-machine checks are complete. Use a focused High review only if a concrete lifetime/concurrency defect emerges.

### N09 — P3: finishing work, not current architecture blockers

- Minimum-width transport rows may overlap; verify at 1000×650 and scaled displays. The inspected 1400×900 screenshot was readable. Missing icon glyphs remain cosmetic.
- The GUI test render is explicitly four seconds and can truncate long release tails. Add configurable duration/tail handling when export becomes a user feature; the current label is honest.
- Telemetry still wakes every 50 ms while idle. Measure idle CPU first; change polling only after evidence.
- Native DLL discovery remains trusted filename/export discovery; manifest enablement, dependency/update/disable lifecycle is future SDK work.
- Pi/ARM, polyphony, full legacy migration, CHAOS and effects remain unimplemented/unverified. Keep preserved legacy/private data intact.

## Detailed next-prompt sequence

### Stage A — close launch and workflow uncertainty

One coordinator/implementation agent, no worker by default. Read AGENTS, this note and the checkpoint; avoid rediscovering all legacy files.

1. Preserve the current worktree and record its existing changes; do not overwrite the user mixer or personal data. The implementation and incoming review are not yet committed as a new checkpoint.
2. Add N01 readiness/logging and record a normal desktop launch. Retain audio-off startup.
3. Resolve N03 continuation behavior and N04 release/focus behavior with focused Qt tests.
4. Refresh the post-build/test metadata record using the improved helper, then run the focused Qt checks. One broad CTest/UI milestone after these fixes is sufficient.

**Gate:** normal app readiness is evidenced; save/focus failures are reproducible and covered; audio stays under explicit user control. User's present “audio works” report remains the accepted starting state.

### Stage B — make larger patches predictable

1. Resolve N02 quiescent setup versus live event budgets before adding parameter-heavy instruments.
2. Add N05 shared scalar-transition tests/behavior where appropriate, including mixer levels.
3. Add small deterministic stress scenarios: many nodes, many simultaneous parameter events, dense timestamps, gate/note traffic, overload/recovery. Report actual percentiles, event/spans, underruns/faults and quality settings.
4. Benchmark against the present native workload with identical settings. Existing tiny steady-tone benchmarks do not establish practical capacity or a CHAOS speedup. Do not optimize around a timing result until the workload is representative.

**Gate:** setup cannot unexpectedly trip the live budget; overload is explicit/recoverable; accepted state after restart matches saved state; core and module partition tests remain green.

### Stage C — harden automation and portable deployment

N06/N07 are independent of native DSP after interfaces settle. If delegation materially helps, give **one Luna Medium worker** ownership of `tools/` and its tests; coordinator owns native/controller work. State ownership and acceptance before spawning; return concise findings rather than full logs.

1. Owned process-tree timeout cleanup, actual-build provenance and artifact-layout tests.
2. Package the same accepted binaries; normal portable launch and isolated-path smoke; record dependency versions/hashes and remaining release inventory.
3. Clean Windows test when a clean environment is available. Do not substitute another smoke on this development machine and call it clean-machine proof.

**Gate:** failures terminate boundedly with useful logs; test/package records identify actual inputs/outputs; app launches without a local Qt SDK in a clean environment before any broad distribution claim.

### Stage D — bounded live-device validation, then Pi feasibility

User-initiated audio only. Record one selected device/backend first, not an exhaustive matrix. Suggested bounded sequence: repeated start/stop, note/gate/focus/panic, stopped device switching, and a timed sustained test with counters. If a failure occurs, retain error/session evidence and attempt at most two focused fixes before escalating. Validate alternative format support without silently mismatching engine/device rates.

Only after that Windows checkpoint, cross-build the headless engine/tests for the actual Pi target and measure on hardware, then validate the standalone Qt app there. No Pi promise follows from portable C++ source alone.

### Stage E — next feature decision

After the previous gates, choose one small additional useful module or a narrow CHAOS feasibility slice. Reuse the generic module/parameter/lifecycle contracts. Do not start a full CHAOS port, effect suite, live graph swapping, broad runtime bake-off or GUI redesign merely because basic audio now works. Escalate architectural review only when measurements or a concrete requirement challenge a settled decision.

## Verification evidence from the implementation stage

All paths below are relative to `build_reports/foundation/`:

- Release build: `build-20261026T070943Z.json` — passed.
- Four CTest suites: `test-20261026T070752Z.json`, raw `test-ctest-20261026T070750Z.log` — all passed, including the final smoothing/partition additions. No later C++ source change was made. The metadata-refresh helper was added afterward; see N07.
- Eight Python helper tests — passed after the final tooling edits (three original, five new).
- Qt smoke: `ui-test-20261026T070402Z.json` — passed, audio not started.
- Offline benchmark: `benchmark-20261026T070643Z.json` — 15 basic rate/block scenarios, 256 blocks; no hardware or capacity conclusion.
- Offline render: `render-20261026T070644Z.json` — passed; WAV was not played by the agent.
- Final portable package: `package-20261026T071008Z.json` — passed, includes `build/packages/wirescape-release-20261026T070953Z/` and file hashes. Isolated-path offscreen check only.
- Source fingerprint in final build/package/launch records: `0d7c797dc5d158ba40e54198a30b330ed6ba6c21edf144603faa518964366f0e`; scope is native/tool source, not a complete hermetic environment.
- User live test: sound passes and output meter moves. Actual device/timing/session details not supplied.

The old “roughly 10,000 checks” counter is not a count of distinct cases. Allocation instrumentation observes ordinary C++ new/new[] in the measured static engine/stock module path; it does not prove arbitrary third-party/native allocations absent.

## Agents, quota and handoff

Implementation used two explicitly authorized independent workers, both `gpt-6.1-sol/medium`: document/controller/QML and device adapter/mock tests. Coordinator handled engine/modules/tooling/build/package integration. No High, Ultra or Max worker was spawned. Follow-up review used no new worker. The current chat's model setting was not changed by tools.

Observed account telemetry during this milestone: primary 25% used over a 300-minute window; secondary 4% used over a 10080-minute window. These are shared account measurements, not this task's usage delta or per-agent token totals.

Next agent should begin Stage A, preserve settled decisions and the working audio/mixer result, and avoid re-running unrelated research. Any implementation must wait for the user's next prompt.
